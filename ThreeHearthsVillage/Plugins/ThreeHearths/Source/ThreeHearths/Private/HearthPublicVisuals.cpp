#include "HearthVillage.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString PublicMeshPath(const FString& Asset)
    {
        return FString::Printf(TEXT("/Game/ThreeHearths/Generated/PublicWallKit/%s/%s.%s"), *Asset, *Asset, *Asset);
    }

    TSharedRef<FJsonObject> PublicPartJson(const FHearthPublicPart& Part)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("id"), Part.Id);
        Json->SetStringField(TEXT("asset"), Part.Asset);
        Json->SetStringField(TEXT("task_id"), Part.TaskId);
        Json->SetStringField(TEXT("status"), Part.Status);
        Json->SetNumberField(TEXT("stage"), Part.Stage);
        Json->SetNumberField(TEXT("worker"), Part.Worker);
        Json->SetStringField(TEXT("offset"), Part.Offset.ToString());
        TArray<TSharedPtr<FJsonValue>> Required;
        TArray<TSharedPtr<FJsonValue>> Reserved;
        TArray<TSharedPtr<FJsonValue>> Delivered;
        for (int32 Material = 0; Material < 3; ++Material)
        {
            Required.Add(MakeShared<FJsonValueNumber>(Part.Required[Material]));
            Reserved.Add(MakeShared<FJsonValueNumber>(Part.Reserved[Material]));
            Delivered.Add(MakeShared<FJsonValueNumber>(Part.Delivered[Material]));
        }
        Json->SetArrayField(TEXT("required"), Required);
        Json->SetArrayField(TEXT("reserved"), Reserved);
        Json->SetArrayField(TEXT("delivered"), Delivered);
        return Json;
    }

    TSharedRef<FJsonObject> PublicOrderJson(const FHearthSupplyOrder& Order)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("id"), Order.Id);
        Json->SetStringField(TEXT("project_id"), Order.ProjectId);
        Json->SetStringField(TEXT("status"), Order.Status);
        Json->SetStringField(TEXT("result"), Order.Result);
        Json->SetStringField(TEXT("origin"), Order.Origin);
        Json->SetNumberField(TEXT("seller"), Order.Seller);
        Json->SetNumberField(TEXT("quantity"), Order.Quantity);
        Json->SetNumberField(TEXT("price"), Order.Price);
        Json->SetNumberField(TEXT("reserved_quantity"), Order.ReservedQuantity);
        Json->SetNumberField(TEXT("escrow"), Order.Escrow);
        Json->SetNumberField(TEXT("remaining"), Order.Remaining);
        return Json;
    }
}

void AHearthVillage::RefreshPublicVisuals()
{
    const bool bHasSite = PublicProject.Site >= 0 && ProductionSites.IsValidIndex(PublicProject.Site);
    const int32 Completed = PublicProject.Completed;
    FString CachedProjectId;
    if (PublicMeshes.Num() > 0 && PublicMeshes[0])
        for (const FName& Tag : PublicMeshes[0]->ComponentTags)
            if (Tag.ToString().StartsWith(TEXT("PublicProject="))) CachedProjectId = Tag.ToString().RightChop(14);

    const bool bCacheMatches = bHasSite && !PublicProject.Id.IsEmpty()
        && CachedProjectId == PublicProject.Id && PublicVisualCount == Completed;
    if (!bCacheMatches)
    {
        for (TObjectPtr<UStaticMeshComponent>& Mesh : PublicMeshes)
            if (IsValid(Mesh)) Mesh->DestroyComponent();
        PublicMeshes.Reset();
        PublicVisualCount = Completed;
    }
    if (!bHasSite || PublicProject.Id.IsEmpty() || Completed <= 0) return;
    if (bCacheMatches) return;

    const FHearthSite& Site = ProductionSites[PublicProject.Site];
    const int32 Limit = FMath::Min(Completed, PublicProject.Parts.Num());
    for (int32 Index = 0; Index < Limit; ++Index)
    {
        const FHearthPublicPart& Part = PublicProject.Parts[Index];
        if (Part.Status != TEXT("completed")) continue;
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
        if (!IsValid(Mesh)) continue;
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetWorldScale3D(FVector(1.f));
        Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("PublicProject=%s"), *PublicProject.Id)));
        if (UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, *PublicMeshPath(Part.Asset)))
        {
            Mesh->SetStaticMesh(Asset);
            Mesh->SetWorldLocation(Site.Position + Part.Offset);
            Mesh->SetVisibility(true);
            Mesh->RegisterComponent();
            PublicMeshes.Add(Mesh);
        }
        else
        {
            Mesh->DestroyComponent();
        }
    }
}

FString AHearthVillage::PublicWorksSummary() const
{
    if (PublicProject.Status == TEXT("unapproved")) return TEXT("公共工程：待国王批准");
    int32 ActiveOrders = 0;
    int32 ActiveEscrow = 0;
    for (const FHearthSupplyOrder& Order : PublicProject.Orders)
        if (Order.Status == TEXT("transporting")) { ++ActiveOrders; ActiveEscrow += Order.Escrow; }
    const int32 Total = PublicProject.Parts.Num();
    const TCHAR* Status = PublicProject.Status == TEXT("completed") ? TEXT("已完工") : TEXT("建设中");
    return FString::Printf(TEXT("公共城墙：%s %d/%d · 石%d 木板%d 房梁%d · 供应单%d/托管%d · 固定税率%d%%"),
        Status, PublicProject.Completed, Total, PublicProject.Stock[0], PublicProject.Stock[1], PublicProject.Stock[2],
        ActiveOrders, ActiveEscrow, TaxRatePercent);
}

void AHearthVillage::AddPublicSnapshot(const TSharedRef<FJsonObject>& Root) const
{
    TSharedRef<FJsonObject> Project = MakeShared<FJsonObject>();
    Project->SetStringField(TEXT("id"), PublicProject.Id);
    Project->SetStringField(TEXT("template_id"), PublicProject.TemplateId);
    Project->SetStringField(TEXT("policy"), PublicProject.Policy);
    Project->SetStringField(TEXT("status"), PublicProject.Status);
    Project->SetStringField(TEXT("approval_history_id"), PublicProject.ApprovalHistoryId);
    Project->SetNumberField(TEXT("king"), PublicProject.King);
    Project->SetNumberField(TEXT("site"), PublicProject.Site);
    Project->SetNumberField(TEXT("approved_at"), PublicProject.ApprovedAt);
    Project->SetNumberField(TEXT("completed"), PublicProject.Completed);
    TArray<TSharedPtr<FJsonValue>> Stock;
    TArray<TSharedPtr<FJsonValue>> Grants;
    for (int32 Material = 0; Material < 3; ++Material)
    {
        Stock.Add(MakeShared<FJsonValueNumber>(PublicProject.Stock[Material]));
        Grants.Add(MakeShared<FJsonValueNumber>(PublicProject.Grants[Material]));
    }
    Project->SetArrayField(TEXT("stock"), Stock);
    Project->SetArrayField(TEXT("grants"), Grants);

    TArray<TSharedPtr<FJsonValue>> Parts;
    for (const FHearthPublicPart& Part : PublicProject.Parts)
        Parts.Add(MakeShared<FJsonValueObject>(PublicPartJson(Part)));
    Project->SetArrayField(TEXT("parts"), Parts);
    TArray<TSharedPtr<FJsonValue>> Orders;
    int32 ActiveEscrow = 0;
    for (const FHearthSupplyOrder& Order : PublicProject.Orders)
    {
        Orders.Add(MakeShared<FJsonValueObject>(PublicOrderJson(Order)));
        if (Order.Status == TEXT("transporting")) ActiveEscrow += Order.Escrow;
    }
    Project->SetArrayField(TEXT("orders"), Orders);
    Project->SetNumberField(TEXT("active_escrow"), ActiveEscrow);
    Project->SetStringField(TEXT("summary_zh"), PublicWorksSummary());
    Root->SetObjectField(TEXT("public_project"), Project);

    Root->SetNumberField(TEXT("general_funds"), GeneralFunds());
    Root->SetNumberField(TEXT("protected_funds"), TaxProjectCoins);
    Root->SetNumberField(TEXT("total_active_escrow"), ActiveEscrow);
}
