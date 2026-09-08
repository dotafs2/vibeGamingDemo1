#if WITH_DEV_AUTOMATION_TESTS

#include "HearthMedievalPersistence.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthMedievalPersistenceTest,
    "ThreeHearths.MedievalPersistence.RoundTripAndAtomicValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    FMedievalSocietyState MakeFixture()
    {
        using namespace HearthMedievalSociety;
        FMedievalSocietyState State;
        FString Error;
        FMedievalSocietyMember Gate; Gate.StableId=TEXT("gatekeeper-01"); Gate.DisplayName=TEXT("Gatekeeper"); Gate.Role=EMedievalSocietyRole::Gatekeeper;
        FMedievalSocietyMember Carter; Carter.StableId=TEXT("carter-01"); Carter.DisplayName=TEXT("Carter"); Carter.Role=EMedievalSocietyRole::Carter;
        AddMember(State,Gate,&Error); AddMember(State,Carter,&Error);
        FMedievalHorse Horse; Horse.StableId=TEXT("horse-01"); AddHorse(State,Horse,&Error);
        FMedievalCart Cart; Cart.StableId=TEXT("cart-01"); Cart.OperatorStableId=Carter.StableId; Cart.HorseStableId=Horse.StableId; Cart.CapacityUnits=5; AddCart(State,Cart,&Error);
        FMedievalStorage Source; Source.StableId=TEXT("store-source"); Source.Inventory.Add(TEXT("wood"),5); Source.Inventory.Add(TEXT("stone"),2);
        FMedievalStorage Destination; Destination.StableId=TEXT("store-destination");
        AddStorage(State,Source,&Error); AddStorage(State,Destination,&Error);
        FMedievalVisitorResult Visitor;
        CheckInVisitor(State,TEXT("visitor-01"),TEXT("Merchant"),false,true,Gate.StableId,TEXT("visit-01"),Visitor);
        TMap<FString,int32> Cargo; Cargo.Add(TEXT("wood"),3); Cargo.Add(TEXT("stone"),1);
        FMedievalFreightResult Freight;
        ReservePickup(State,TEXT("freight-01"),Cart.StableId,Carter.StableId,Source.StableId,Destination.StableId,Cargo,Freight);
        CompletePickup(State,TEXT("freight-01"),Freight);
        return State;
    }

    bool IsUnchanged(const FMedievalSocietyState& State, const FString& Snapshot)
    {
        FString Current;
        return HearthMedievalPersistence::Serialize(State,Current) && Current==Snapshot;
    }

    FString InvalidItemJson(const FString& Material, const FString& Amount)
    {
        return FString::Printf(TEXT("{\"schema_version\":1,\"revision\":0,\"members\":[],\"visitors\":[],\"events\":[],\"storages\":[{\"stable_id\":\"store\",\"inventory\":[{\"id\":\"%s\",\"amount\":%s}],\"reserved\":[]}],\"horses\":[],\"carts\":[],\"freight_reservations\":[]}"), *Material, *Amount);
    }
}

bool FHearthMedievalPersistenceTest::RunTest(const FString&)
{
    using namespace HearthMedievalPersistence;
    using namespace HearthMedievalSociety;
    FString Json;
    FMedievalSocietyState State=MakeFixture();
    TestTrue(TEXT("fixture is valid before save"),ValidateState(State));
    TestTrue(TEXT("complete pickup state serializes"),Serialize(State,Json));
    TestTrue(TEXT("serialized state includes schema version"),Json.Contains(TEXT("schema_version")));

    FMedievalSocietyState Reloaded;
    FString Error;
    TestTrue(TEXT("serialized state deserializes"),Deserialize(Json,Reloaded,Error));
    FString Reserialized;
    TestTrue(TEXT("reloaded state serializes"),Serialize(Reloaded,Reserialized));
    TestEqual(TEXT("round trip is deterministic"),Reserialized,Json);
    TestTrue(TEXT("visitor event history survives reload"),Reloaded.Events.ContainsByPredicate([](const FMedievalSocietyEvent& E){ return E.EventId==TEXT("visit-01"); }));
    TestTrue(TEXT("picked-up reservation survives reload"),Reloaded.FreightReservations.ContainsByPredicate([](const FMedievalFreightReservation& R){ return R.ReservationId==TEXT("freight-01") && R.Stage==EMedievalFreightStage::PickedUp; }));
    const FMedievalSocietyMember* ReloadedCarter=FindMember(Reloaded,TEXT("carter-01"));
    TestNotNull(TEXT("member ownership and emotion survive reload"),ReloadedCarter);
    if(ReloadedCarter)
    {
        TestEqual(TEXT("carter role survives reload"),ReloadedCarter->Role,EMedievalSocietyRole::Carter);
        TestEqual(TEXT("carter morale survives reload"),ReloadedCarter->Emotion.Morale,60.f);
    }
    const FMedievalCart* ReloadedCart=Reloaded.Carts.FindByPredicate([](const FMedievalCart& C){ return C.StableId==TEXT("cart-01"); });
    TestNotNull(TEXT("cart ownership survives reload"),ReloadedCart);
    if(ReloadedCart)
    {
        TestEqual(TEXT("cart owner survives reload"),ReloadedCart->OperatorStableId,FString(TEXT("carter-01")));
        TestEqual(TEXT("cart cargo survives reload"),ReloadedCart->Cargo.FindRef(TEXT("wood")),3);
    }
    const int32 ReloadedEventCount=Reloaded.Events.Num();
    FMedievalVisitorResult Visitor;
    TestTrue(TEXT("duplicate visitor check-in remains idempotent after reload"),CheckInVisitor(Reloaded,TEXT("visitor-01"),TEXT("Changed Name"),false,false,TEXT("gatekeeper-01"),TEXT("visit-01"),Visitor) && Visitor.bAlreadyApplied);
    TestEqual(TEXT("reloaded visitor history is not duplicated"),Reloaded.Events.Num(),ReloadedEventCount);

    FMedievalFreightResult Freight;
    TestTrue(TEXT("delivery after reload transfers freight"),CompleteDelivery(Reloaded,TEXT("freight-01"),Freight));
    TestTrue(TEXT("delivery after reload reports moved units"),Freight.MovedUnits==4);
    TestTrue(TEXT("repeated delivery after reload is idempotent"),CompleteDelivery(Reloaded,TEXT("freight-01"),Freight) && Freight.bAlreadyApplied);
    const FMedievalStorage* Destination=Reloaded.Storages.FindByPredicate([](const FMedievalStorage& S){ return S.StableId==TEXT("store-destination"); });
    const FMedievalStorage* Source=Reloaded.Storages.FindByPredicate([](const FMedievalStorage& S){ return S.StableId==TEXT("store-source"); });
    TestNotNull(TEXT("destination storage survives reload"),Destination);
    TestNotNull(TEXT("source storage survives reload"),Source);
    if(Destination && Source)
    {
        TestEqual(TEXT("delivery adds exactly three wood"),Destination->Inventory.FindRef(TEXT("wood")),3);
        TestEqual(TEXT("delivery adds exactly one stone"),Destination->Inventory.FindRef(TEXT("stone")),1);
        TestEqual(TEXT("source wood is not deducted twice"),Source->Inventory.FindRef(TEXT("wood")),2);
    }

    FString StableSnapshot;
    TestTrue(TEXT("fixture snapshot serializes"),Serialize(State,StableSnapshot));
    const TArray<TPair<FString,FString>> Invalids = {
        {TEXT("unknown material is rejected"), InvalidItemJson(TEXT("unobtanium"),TEXT("1"))},
        {TEXT("negative material amount is rejected"), InvalidItemJson(TEXT("wood"),TEXT("-1"))},
        {TEXT("integer overflow is rejected"), InvalidItemJson(TEXT("wood"),TEXT("2147483648"))}
    };
    for(const TPair<FString,FString>& Invalid : Invalids)
    {
        FMedievalSocietyState Target=State;
        TestFalse(Invalid.Key,Deserialize(Invalid.Value,Target,Error));
        TestTrue(FString::Printf(TEXT("target remains unchanged after %s"),*Invalid.Key),IsUnchanged(Target,StableSnapshot));
    }

    // Mutate parsed fields so pretty-print whitespace cannot silently turn an
    // invalid-input fixture into an unchanged valid document.
    auto MutateJson=[this](const FString& Input,TFunction<void(const TSharedRef<FJsonObject>&)> Mutate)
    {
        TSharedPtr<FJsonObject> Root;
        if(!TestTrue(TEXT("mutation fixture parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Root))) return FString();
        Mutate(Root.ToSharedRef());
        FString Result; FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Result));
        return Result;
    };
    const FString OrphanEscrowJson=MutateJson(Json,[](const TSharedRef<FJsonObject>& Root)
    {
        auto Item=MakeShared<FJsonObject>(); Item->SetStringField(TEXT("id"),TEXT("wood")); Item->SetNumberField(TEXT("amount"),1);
        Root->GetArrayField(TEXT("storages"))[0]->AsObject()->SetArrayField(TEXT("reserved"),{MakeShared<FJsonValueObject>(Item)});
    });
    FMedievalSocietyState EscrowTarget=State;
    TestFalse(TEXT("deserializer rejects orphaned storage escrow"),Deserialize(OrphanEscrowJson,EscrowTarget,Error));
    TestTrue(TEXT("orphaned escrow cannot partially replace target"),IsUnchanged(EscrowTarget,StableSnapshot));

    const FString OrphanCartJson=MutateJson(Json,[](const TSharedRef<FJsonObject>& Root)
    {
        Root->GetArrayField(TEXT("carts"))[0]->AsObject()->SetStringField(TEXT("active_reservation_id"),TEXT(""));
    });
    FMedievalSocietyState CartTarget=State;
    TestFalse(TEXT("deserializer rejects cargo without active reservation"),Deserialize(OrphanCartJson,CartTarget,Error));
    TestTrue(TEXT("orphaned cargo cannot partially replace target"),IsUnchanged(CartTarget,StableSnapshot));

    FString MissingGuardJson;
    TestTrue(TEXT("guard reference fixture serializes"),Serialize(State,MissingGuardJson));
    MissingGuardJson=MutateJson(MissingGuardJson,[](const TSharedRef<FJsonObject>& Root)
    {
        // Change only the reference; renaming the guard itself would stay valid.
        Root->GetArrayField(TEXT("events"))[0]->AsObject()->SetStringField(TEXT("guard_id"),TEXT("missing-guard"));
    });
    FMedievalSocietyState GuardTarget=State;
    TestFalse(TEXT("missing guard reference is rejected"),Deserialize(MissingGuardJson,GuardTarget,Error));
    TestTrue(TEXT("invalid guard reference does not partially replace state"),IsUnchanged(GuardTarget,StableSnapshot));
    return true;
}

#endif
