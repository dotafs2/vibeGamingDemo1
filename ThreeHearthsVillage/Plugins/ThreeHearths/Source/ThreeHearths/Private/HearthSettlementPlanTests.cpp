#if WITH_DEV_AUTOMATION_TESTS

#include "HearthSettlementPlan.h"
#include "HearthRoyalWorksPlan.h"
#include "Misc/AutomationTest.h"
#include <limits>

namespace HearthSettlementPlanTests
{
    // Test against independent geometric predicates and checkpoint coordinates,
    // rather than accepting the production siting function as a containment test.
    double Cross(const FVector& A, const FVector& B, const FVector& P)
    {
        return (B.X-A.X)*(P.Y-A.Y)-(B.Y-A.Y)*(P.X-A.X);
    }

    bool ContainsXY(const TArray<FVector>& Boundary, const FVector& P)
    {
        int32 Winding=0;
        for (int32 I=0; I<Boundary.Num(); ++I)
        {
            const FVector A(Boundary[I].X,Boundary[I].Y,0);
            const FVector B(Boundary[(I+1)%Boundary.Num()].X,Boundary[(I+1)%Boundary.Num()].Y,0);
            if (FVector::DistSquared(FVector(P.X,P.Y,0),FMath::ClosestPointOnSegment(FVector(P.X,P.Y,0),A,B)) < .01)
                return true;
            if (A.Y<=P.Y && B.Y>P.Y && Cross(A,B,P)>0) ++Winding;
            if (A.Y>P.Y && B.Y<=P.Y && Cross(A,B,P)<0) --Winding;
        }
        return Winding!=0;
    }

    bool ProperCrossing(const FVector& A, const FVector& B, const FVector& C, const FVector& D)
    {
        return Cross(A,B,C)*Cross(A,B,D)<0 && Cross(C,D,A)*Cross(C,D,B)<0;
    }

    bool ContainsBox(const FBox& Outer, const FBox& Inner)
    {
        return Outer.IsValid && Inner.IsValid && Outer.ExpandBy(.1).IsInsideOrOn(Inner.Min)
            && Outer.ExpandBy(.1).IsInsideOrOn(Inner.Max);
    }

    FBox LineBounds(const FHearthSettlementPlan& Plan, const FString& Prefix=FString())
    {
        FBox Box(ForceInit);
        for (const auto& Line : Plan.CastleLines)
            if (Prefix.IsEmpty() || Line.Id.StartsWith(Prefix))
                for (const auto& Point : Line.Points) Box+=Point;
        return Box;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthSettlementDistrictTest,
    "ThreeHearths.SettlementPlan.CheckpointDistricts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthSettlementDistrictTest::RunTest(const FString&)
{
    using namespace HearthSettlementPlanTests;
    const auto Plan=HearthSettlementPlan::Build(FVector(6500,6500,355),TEXT("royal_keep_garden_v2"));
    TestEqual(TEXT("Seven occupied uses plus three explicit future districts"),Plan.Districts.Num(),10);
    for(const auto& Future:Plan.Districts) if(Future.Id.StartsWith(TEXT("future_")))
        TestTrue(TEXT("Unoccupied expansion is visibly labelled planned"),Future.Label.Contains(TEXT("预留")) && Future.Purpose.Contains(TEXT("待发展")));
    const auto* Hub=Plan.Districts.FindByPredicate([](const auto& D){return D.Id==TEXT("market_life");});
    const auto* WestWork=Plan.Districts.FindByPredicate([](const auto& D){return D.Id==TEXT("workshop_west");});
    const auto* SouthField=Plan.Districts.FindByPredicate([](const auto& D){return D.Id==TEXT("production_south_farm");});
    const auto* NorthReserve=Plan.Districts.FindByPredicate([](const auto& D){return D.Id==TEXT("future_north_residential");});
    const auto* EastMeadow=Plan.Districts.FindByPredicate([](const auto& D){return D.Id==TEXT("future_east_life");});
    TestTrue(TEXT("Starting hub is explicitly identified"),Hub && Hub->Label.Contains(TEXT("起始")) && Hub->Purpose.Contains(TEXT("起点")));
    TestTrue(TEXT("West work district explains its forest edge"),WestWork && WestWork->Label.Contains(TEXT("林缘")) && WestWork->Purpose.Contains(TEXT("木工")));
    TestTrue(TEXT("South field district explains food production"),SouthField && SouthField->Label.Contains(TEXT("田野")) && SouthField->Purpose.Contains(TEXT("粮食")));
    TestTrue(TEXT("North reserve stays unexplored and advisory"),NorthReserve && NorthReserve->Label.Contains(TEXT("未探"))
        && NorthReserve->Purpose.Contains(TEXT("勘察")) && !NorthReserve->Purpose.Contains(TEXT("迷宫")));
    TestTrue(TEXT("East future district explains meadow life"),EastMeadow && EastMeadow->Label.Contains(TEXT("草甸")) && EastMeadow->Purpose.Contains(TEXT("牧草")));
    TestTrue(TEXT("Future districts surround central castle on all sides"),
        FMath::Abs(Plan.Bounds.GetCenter().X-6500)<1000 && FMath::Abs(Plan.Bounds.GetCenter().Y-6500)<1000);
    TSet<FString> Ids;
    for (const auto& District : Plan.Districts)
    {
        TestFalse(TEXT("Stable unique district ID"),District.Id.IsEmpty() || Ids.Contains(District.Id));
        Ids.Add(District.Id);
        TestTrue(District.Id+TEXT(": irregular polygon"),District.Boundary.Num()>=5);
        TestTrue(District.Id+TEXT(": concise HUD purpose"),!District.Purpose.IsEmpty() && District.Purpose.Len()<=19);
        for (const TCHAR Ch : District.Purpose)
            TestFalse(TEXT("Purpose has no English words"),(Ch>='a' && Ch<='z') || (Ch>='A' && Ch<='Z'));
        TestTrue(District.Id+TEXT(": label inside its district"),ContainsXY(District.Boundary,District.LabelPosition));
        double TwiceArea=0;
        bool bDiagonal=false;
        for (int32 I=0; I<District.Boundary.Num(); ++I)
        {
            const FVector& A=District.Boundary[I];
            const FVector& B=District.Boundary[(I+1)%District.Boundary.Num()];
            TestFalse(TEXT("Finite planning boundary"),A.ContainsNaN());
            TestFalse(TEXT("No repeated corner"),A.Equals(B,.01));
            TestTrue(TEXT("Camera bounds include every planning point"),Plan.Bounds.ExpandBy(.01).IsInsideOrOn(A));
            TwiceArea+=A.X*B.Y-B.X*A.Y;
            bDiagonal|=FMath::Abs(A.X-B.X)>1 && FMath::Abs(A.Y-B.Y)>1;
            for (int32 J=I+2; J<District.Boundary.Num(); ++J)
            {
                if (I==0 && J==District.Boundary.Num()-1) continue;
                TestFalse(District.Id+TEXT(": no self intersection"),ProperCrossing(A,B,District.Boundary[J],District.Boundary[(J+1)%District.Boundary.Num()]));
            }
        }
        TestTrue(TEXT("Nonzero area without rectangular parcel grid"),FMath::Abs(TwiceArea)>10000 && bDiagonal);
    }
    struct FAnchor { const TCHAR* District; FVector Point; };
    const FAnchor Anchors[]={
        {TEXT("residential_east"),FVector(2763,-581,0)},
        {TEXT("residential_east"),FVector(4566,-1486,0)},
        {TEXT("residential_east"),FVector(4321,-3025,0)},
        {TEXT("residential_east"),FVector(4626,-40,0)},
        {TEXT("market_life"),FVector(-1639,443,0)},
        {TEXT("market_life"),FVector(481,-39,0)},
        {TEXT("market_life"),FVector(-1100,-1050,0)},
        {TEXT("residential_southwest"),FVector(-1616,-4427,0)},
        {TEXT("residential_southwest"),FVector(-4880,-3159,0)},
        {TEXT("residential_southwest"),FVector(-6036,-1396,0)},
        {TEXT("residential_southwest"),FVector(-4379,-4744,0)},
        {TEXT("workshop_west"),FVector(-2250,-1050,0)},
        {TEXT("workshop_west"),FVector(-2800,-1050,0)},
        {TEXT("workshop_west"),FVector(-4300,-1900,0)},
        {TEXT("workshop_west"),FVector(-4300,0,0)},
        {TEXT("workshop_west"),FVector(-4300,1900,0)},
        {TEXT("production_south_farm"),FVector(-1135,-2825,0)},
        {TEXT("production_north_quarry"),FVector(-1000,3100,0)},
        {TEXT("production_north_quarry"),FVector(-200,3100,0)},
        {TEXT("production_north_quarry"),FVector(600,3100,0)},
        {TEXT("castle_northeast"),FVector(6500,6500,355)}
    };
    for (const auto& Anchor : Anchors)
    {
        const auto* District=Plan.Districts.FindByPredicate([&](const auto& D){return D.Id==Anchor.District;});
        if (TestNotNull(FString(Anchor.District)+TEXT(": present"),District))
            TestTrue(FString(Anchor.District)+TEXT(": checkpoint anchor ")+Anchor.Point.ToString(),ContainsXY(District->Boundary,Anchor.Point));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthSettlementCastleEnvelopeTest,
    "ThreeHearths.SettlementPlan.RealCastleEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthSettlementCastleEnvelopeTest::RunTest(const FString&)
{
    using namespace HearthSettlementPlanTests;
    const FVector Site(6500,6500,355);
    const auto Royal=HearthRoyalWorksPlan::BuildForTemplate(TEXT("royal_keep_garden_v2"));
    const auto Plan=HearthSettlementPlan::Build(Site,Royal.TemplateId);
    const FBox Silhouette=LineBounds(Plan);
    if (!TestTrue(TEXT("Castle has three-dimensional planning lines"),Silhouette.IsValid != 0)) return false;
    TestTrue(TEXT("Checkpoint castle full width includes 67m curtain foundations"),FMath::IsNearlyEqual(Silhouette.GetSize().X,6700.0,1.0));
    TestTrue(TEXT("South gate and north curtain span 60.5m"),FMath::IsNearlyEqual(Silhouette.GetSize().Y,6050.0,1.0));
    TestTrue(TEXT("Tower is 31.809m, not a room-height box"),FMath::IsNearlyEqual(Silhouette.Max.Z-Site.Z,3180.9,1.0));
    TestTrue(TEXT("Foundation datum is real site height 355cm"),FMath::IsNearlyEqual(Silhouette.Min.Z,355.0,.1));
    TestTrue(TEXT("Plan camera bounds include entire castle"),ContainsBox(Plan.Bounds,Silhouette));

    int32 Segments=0;
    TSet<FString> Ids;
    for (const auto& Line : Plan.CastleLines)
    {
        TestFalse(TEXT("Outline IDs unique"),Ids.Contains(Line.Id)); Ids.Add(Line.Id);
        TestTrue(TEXT("Renderable polyline"),Line.Points.Num()>=(Line.bClosed?3:2));
        Segments+=Line.Points.Num()-1+(Line.bClosed?1:0);
        TestFalse(TEXT("Unrelated landscaping excluded"),Line.Id.Contains(TEXT("courtyard")) || Line.Id.Contains(TEXT("service")));
        for (const auto& P : Line.Points) TestFalse(TEXT("Finite castle coordinates"),P.ContainsNaN());
    }
    TestTrue(TEXT("Under 250 polylines AND actual line segments"),Plan.CastleLines.Num()<250 && Segments<250 && Segments>30);

    const auto* District=Plan.Districts.FindByPredicate([](const auto& D){return D.Id==TEXT("castle_northeast");});
    if (!TestNotNull(TEXT("Castle district exists"),District)) return false;
    int32 StructureCount=0;
    for (const auto& Module : Royal.Modules)
    {
        if (!Module.PlantId.IsEmpty() || Module.Id.StartsWith(TEXT("courtyard_walk_"))) continue;
        ++StructureCount;
        const FBox Box(Site+Module.Offset-Module.BoundsSizeCm*.5,Site+Module.Offset+Module.BoundsSizeCm*.5);
        TestTrue(Module.Id+TEXT(": true scaled/rotated module bounds covered"),ContainsBox(Silhouette,Box));
        for (int32 C=0; C<4; ++C)
            TestTrue(Module.Id+TEXT(": castle district covers footprint"),ContainsXY(District->Boundary,
                FVector(C&1?Box.Max.X:Box.Min.X,C&2?Box.Max.Y:Box.Min.Y,Site.Z)));
    }
    TestTrue(TEXT("Hundreds of actual modules aggregated into few lines"),StructureCount>1000 && StructureCount>Plan.CastleLines.Num()*5);

    struct FBody { const TCHAR* Prefix; FVector Interior; double MinimumHeight; };
    const FBody Bodies[]={
        {TEXT("main_hall/"),FVector(0,650,100),3000},
        {TEXT("west_wing/"),FVector(-2200,700,100),700},
        {TEXT("east_wing/"),FVector(2200,700,100),700},
        {TEXT("gate_west/"),FVector(-550,-2650,100),550},
        {TEXT("gate_east/"),FVector(550,-2650,100),550},
        {TEXT("curtain_west/"),FVector(-3250,0,100),600},
        {TEXT("curtain_east/"),FVector(3250,0,100),600},
        {TEXT("curtain_north/"),FVector(0,2700,100),600},
        {TEXT("curtain_south_west/"),FVector(-2000,-2300,100),600},
        {TEXT("curtain_south_east/"),FVector(2000,-2300,100),600}
    };
    for (const auto& Body : Bodies)
    {
        const FBox Box=LineBounds(Plan,Body.Prefix);
        TestTrue(FString(Body.Prefix)+TEXT(": grounded major mass"),Box.IsValid && Box.IsInsideOrOn(Site+Body.Interior) && Box.GetSize().Z>=Body.MinimumHeight);
    }
    for (const auto& Room : Royal.Rooms)
    {
        if (Room.Id!=TEXT("throne_hall") && Room.Id!=TEXT("west_side_wing") && Room.Id!=TEXT("east_side_wing")) continue;
        const FBox Box=LineBounds(Plan,TEXT("room/")+Room.Id);
        TestTrue(Room.Id+TEXT(": footprint matches room manifest"),Box.IsValid && FMath::IsNearlyEqual(Box.GetSize().X,Room.Size.X,.1)
            && FMath::IsNearlyEqual(Box.GetSize().Y,Room.Size.Y,.1));
        TestTrue(Room.Id+TEXT(": follows native floor top, not schematic room Z"),Box.IsValid && FMath::IsNearlyEqual(Box.Min.Z,Site.Z+80,.1));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthSettlementTranslationTest,
    "ThreeHearths.SettlementPlan.SiteTranslationAndTemplates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthSettlementTranslationTest::RunTest(const FString&)
{
    const FVector Site(6500,6500,355),Delta(-2300,1700,875);
    const auto First=HearthSettlementPlan::Build(Site,TEXT("royal_keep_garden_v2"));
    const auto Repeat=HearthSettlementPlan::Build(Site,TEXT("royal_keep_garden_v2"));
    const auto Moved=HearthSettlementPlan::Build(Site+Delta,TEXT("royal_keep_garden_v2"));
    TestEqual(TEXT("Stable line count"),First.CastleLines.Num(),Repeat.CastleLines.Num());
    TestEqual(TEXT("Moving site retains outline topology"),First.CastleLines.Num(),Moved.CastleLines.Num());
    if (First.CastleLines.Num()!=Moved.CastleLines.Num() || First.CastleLines.Num()!=Repeat.CastleLines.Num()) return false;
    for (int32 I=0; I<First.CastleLines.Num(); ++I)
    {
        const auto& A=First.CastleLines[I]; const auto& B=Moved.CastleLines[I]; const auto& C=Repeat.CastleLines[I];
        TestTrue(TEXT("Stable line identity and closure"),A.Id==B.Id && A.Id==C.Id && A.bClosed==B.bClosed);
        if (!TestEqual(TEXT("Stable point count"),A.Points.Num(),B.Points.Num())) continue;
        if (!TestEqual(TEXT("Deterministic point count"),A.Points.Num(),C.Points.Num())) continue;
        for (int32 J=0; J<A.Points.Num(); ++J)
        {
            TestTrue(TEXT("XYZ translation applied exactly once"),B.Points[J].Equals(A.Points[J]+Delta,.01));
            TestTrue(TEXT("No stateful/random layout"),C.Points[J].Equals(A.Points[J],.001));
        }
    }
    for (int32 I=0; I<First.Districts.Num() && I<Moved.Districts.Num(); ++I)
    {
        const auto& A=First.Districts[I]; const auto& B=Moved.Districts[I];
        const FVector Expected=A.Id==TEXT("castle_northeast")?Delta:FVector::ZeroVector;
        TestTrue(TEXT("Existing settlement labels do not move with castle"),B.LabelPosition.Equals(A.LabelPosition+Expected,.01));
        if (TestEqual(TEXT("District topology stays stable"),A.Boundary.Num(),B.Boundary.Num()))
            for (int32 J=0; J<A.Boundary.Num(); ++J)
                TestTrue(TEXT("Only castle district follows site"),B.Boundary[J].Equals(A.Boundary[J]+Expected,.01));
    }
    const auto Unknown=HearthSettlementPlan::Build(Site,TEXT("unrecognized_future_castle"));
    TestTrue(TEXT("Unknown template never silently manufactures v2"),Unknown.CastleLines.IsEmpty());
    const auto Legacy=HearthSettlementPlan::Build(Site,TEXT("royal_keep_garden_v1"));
    TestTrue(TEXT("Legacy manifest remains small, not upgraded to v2"),!Legacy.CastleLines.IsEmpty()
        && HearthSettlementPlanTests::LineBounds(Legacy).GetSize().X<1500);
    const auto Invalid=HearthSettlementPlan::Build(FVector(0,0,std::numeric_limits<double>::quiet_NaN()),TEXT("royal_keep_garden_v2"));
    TestTrue(TEXT("Invalid site cannot poison camera bounds"),Invalid.CastleLines.IsEmpty() && Invalid.Bounds.IsValid && !Invalid.Bounds.Min.ContainsNaN());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthSettlementSitingTest,
    "ThreeHearths.SettlementPlan.SoftRolePreferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthSettlementSitingTest::RunTest(const FString&)
{
    struct FPreference { const TCHAR* Role; FVector Near; FVector Far; };
    const FPreference Preferences[]={
        {TEXT("木匠"),FVector(-2250,-1050,8),FVector(4500,-1500,8)},
        {TEXT("陶工"),FVector(-2800,-1050,8),FVector(-200,3100,8)},
        {TEXT("铁匠"),FVector(-3250,-1400,8),FVector(4500,-1500,8)},
        {TEXT("农民"),FVector(-1135,-2825,8),FVector(-200,3100,8)},
        {TEXT("石匠"),FVector(-200,3100,8),FVector(-1135,-2825,8)},
        {TEXT("商人"),FVector(-1100,-1050,8),FVector(4500,-1500,8)},
        {TEXT("车夫"),FVector(-1100,-1050,8),FVector(4500,-1500,8)},
        {TEXT("国王"),FVector(6500,6500,355),FVector(-4880,-3159,8)},
        {TEXT("门卫"),FVector(6500,3850,355),FVector(-1135,-2825,8)},
        {TEXT("王室护卫"),FVector(6500,6500,355),FVector(-1135,-2825,8)},
        {TEXT("学徒"),FVector(2763,-581,8),FVector(-200,3100,8)},
        {TEXT("织工"),FVector(4321,-3025,8),FVector(-200,3100,8)},
        {TEXT("勘察者"),FVector(6500,17600,8),FVector(-4880,-3159,8)}
    };
    for (const auto& P : Preferences)
    {
        const float Near=HearthSettlementPlan::SitingPenalty(P.Role,P.Near);
        const float Far=HearthSettlementPlan::SitingPenalty(P.Role,P.Far);
        TestEqual(FString(P.Role)+TEXT(": inside preferred use"),Near,0.f);
        TestTrue(FString(P.Role)+TEXT(": expected land-use preference"),Near<Far);
        TestTrue(TEXT("Only bounded soft penalty, never a relocation veto"),FMath::IsFinite(Far) && Far<=8.f);
        TestEqual(TEXT("Independent of terrain elevation"),Near,HearthSettlementPlan::SitingPenalty(P.Role,P.Near+FVector(0,0,2500)));
    }
    for (const FVector& Existing : {FVector(-4880,-3159,8),FVector(-6036,-1396,8),FVector(-1616,-4427,8)})
        TestEqual(TEXT("Generic residents retain west/southwest mixed housing options"),HearthSettlementPlan::SitingPenalty(TEXT("居民"),Existing),0.f);
    TestTrue(TEXT("Generic residents do not consume the northern unknown reserve"),
        HearthSettlementPlan::SitingPenalty(TEXT("居民"),FVector(6500,17600,8))>0.f);
    // A boundary vertex is allowed and a 1cm perturbation cannot cause a jump.
    const FVector Boundary(-1950,-1700,8);
    const float At=HearthSettlementPlan::SitingPenalty(TEXT("木匠"),Boundary);
    TestEqual(TEXT("Boundary itself is included"),At,0.f);
    for (const FVector& Delta : {FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0)})
        TestTrue(TEXT("Preference continuous across district edge"),FMath::Abs(HearthSettlementPlan::SitingPenalty(TEXT("木匠"),Boundary+Delta)-At)<.01f);
    for (const FVector& Bad : {FVector(1.e9,-1.e9,0),FVector(std::numeric_limits<double>::infinity(),0,0),FVector(0,std::numeric_limits<double>::quiet_NaN(),0)})
        TestEqual(TEXT("Remote and invalid candidates saturate safely"),HearthSettlementPlan::SitingPenalty(TEXT("木匠"),Bad),8.f);
    return true;
}

#endif
