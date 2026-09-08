#if WITH_DEV_AUTOMATION_TESTS

#include "HearthRoyalHill.h"
#include "HearthOrganicTerrain.h"
#include "HearthCityPlan.h"
#include "Misc/AutomationTest.h"

namespace HearthRoyalHillTests
{
    using namespace HearthOrganicTerrain;

    double RadiusToSegment(const FVector& Center, const FVector& A, const FVector& B)
    {
        const FVector C(Center.X,Center.Y,0), Start(A.X,A.Y,0), End(B.X,B.Y,0);
        return FVector::Dist2D(C,FMath::ClosestPointOnSegment(C,Start,End));
    }

    // Interpolate the actual A-C-B / B-C-D triangles emitted by GenerateGrid,
    // not HeightAt and not a bilinear approximation of a different surface.
    double MeshHeight(const FGrid& Grid, const FSettings& Settings, const FVector2D& P)
    {
        const double GX=FMath::Clamp((P.X-Settings.Bounds.Min.X)/(Settings.Bounds.Max.X-Settings.Bounds.Min.X)*Settings.GridQuadsX,0.0,double(Settings.GridQuadsX));
        const double GY=FMath::Clamp((P.Y-Settings.Bounds.Min.Y)/(Settings.Bounds.Max.Y-Settings.Bounds.Min.Y)*Settings.GridQuadsY,0.0,double(Settings.GridQuadsY));
        const int32 X=FMath::Min(FMath::FloorToInt(GX),Settings.GridQuadsX-1);
        const int32 Y=FMath::Min(FMath::FloorToInt(GY),Settings.GridQuadsY-1);
        const double U=GX-X,V=GY-Y;
        const int32 A=Y*Grid.VertexColumns+X,B=A+1,C=A+Grid.VertexColumns,D=C+1;
        return U+V<=1 ? Grid.Vertices[A].Z*(1-U-V)+Grid.Vertices[B].Z*U+Grid.Vertices[C].Z*V
            : Grid.Vertices[B].Z*(1-V)+Grid.Vertices[C].Z*(1-U)+Grid.Vertices[D].Z*(U+V-1);
    }

    FSettings WithLegacyFeatures()
    {
        FSettings Settings;
        // Match the parent contract: skip the 600cm visual-only royal segments,
        // ordinary road heights are taken from the legacy natural terrain.
        const FSettings Natural;
        for (const auto& Segment : HearthCityPlan::BuildVersion4().Roads)
        {
            if (Segment.Width>=HearthRoyalHill::RoadWidth) continue;
            FRoadCenterline Road;
            Road.Width=Segment.Width;
            const FVector2D A(Segment.A.X,Segment.A.Y),B(Segment.B.X,Segment.B.Y);
            Road.Nodes={{A,HeightAt(A,Natural)},{B,HeightAt(B,Natural)}};
            Settings.Roads.Add(MoveTemp(Road));
        }
        // Representative old home pads retain their inherited elevations.
        for (const FVector2D& P : {FVector2D(2763,-581),FVector2D(4566,-1486),FVector2D(4321,-3025),FVector2D(4626,-40),
            FVector2D(-1639,443),FVector2D(481,-39),FVector2D(-1616,-4427),FVector2D(-4880,-3159),FVector2D(-6036,-1396),FVector2D(-4379,-4744)})
        {
            FFlattenZone Pad;
            Pad.Center=P; Pad.HalfSize=FVector2D(560,650); Pad.Elevation=HeightAt(P,Natural);
            Settings.FlattenZones.Add(Pad);
        }
        HearthRoyalHill::AddToTerrain(Settings);
        return Settings;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthRoyalHillPlateauTest,
    "ThreeHearths.RoyalHill.PlateauAndLegacyCompatibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthRoyalHillPlateauTest::RunTest(const FString&)
{
    using namespace HearthOrganicTerrain;
    FSettings Legacy,Hill;
    TestFalse(TEXT("Legacy callers do not opt in"),Legacy.bRoyalHill);
    Hill.bRoyalHill=true;
    const auto C=HearthRoyalHill::Center();
    TestTrue(TEXT("Castle is exactly at the 300m map centre"),C.Equals((Hill.Bounds.Min+Hill.Bounds.Max)*.5,.001));
    TestTrue(TEXT("Central highland towers above old terrain"),HeightAt(C,Hill)-HeightAt(C,Legacy)>2800);
    for (int32 Angle=0; Angle<32; ++Angle)
    {
        const double A=2*UE_PI*Angle/32;
        const FVector2D Direction(FMath::Cos(A),FMath::Sin(A));
        for (double Radius : {0.0,4500.0,4800.0})
            TestEqual(TEXT("Whole castle footprint fits on a 3500cm plateau"),HeightAt(C+Direction*Radius,Hill),3500.f);
        for (double Radius : {9000.0,10500.0,14000.0})
            TestEqual(TEXT("Legacy natural terrain preserved outside foot"),HeightAt(C+Direction*Radius,Hill),HeightAt(C+Direction*Radius,Legacy));
        for (double Radius : {4800.0,9000.0})
            TestTrue(TEXT("Radial plateau/foot seams are continuous"),FMath::Abs(HeightAt(C+Direction*(Radius-.1),Hill)-HeightAt(C+Direction*(Radius+.1),Hill))<.1);
    }
    HearthRoyalHill::AddToTerrain(Hill);
    for (int32 Angle=0; Angle<32; ++Angle)
    {
        const double A=2*UE_PI*Angle/32;
        TestEqual(TEXT("Road shoulders cannot cut into the castle plateau"),HeightAt(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*4800,Hill),3500.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthRoyalHillRouteTest,
    "ThreeHearths.RoyalHill.AscentContractAndSafeDelivery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthRoyalHillRouteTest::RunTest(const FString&)
{
    using namespace HearthOrganicTerrain;
    const auto Route=HearthRoyalHill::AscentRoute();
    if (!TestTrue(TEXT("Complete sampled ascent exists"),Route.Num()>200)) return false;
    TestTrue(TEXT("Exact southern toe XY"),FVector2D(Route[0].X,Route[0].Y).Equals(FVector2D(6500,-1900),.001));
    TestEqual(TEXT("Toe Z samples raw legacy height without rounding"),float(Route[0].Z),HeightAt(FVector2D(6500,-1900)));
    TestTrue(TEXT("Level preview reaches south gate"),Route.Last().Equals(FVector(6500,2900,3500),.001));
    const auto Delivery=HearthRoyalHill::DeliveryApproach();
    TestTrue(TEXT("Unload point is outside public site plus 25cm clearance"),Delivery.Y<6500-5000-25 && Delivery.Z==3500);
    TestTrue(TEXT("Delivery is an explicit ascent node"),Route.ContainsByPredicate([&](const FVector& P){return P.Equals(Delivery,.001);}));
    TestTrue(TEXT("Old 1500 gate datum remains level preview geometry"),Route.ContainsByPredicate([](const FVector& P){return P.Equals(FVector(6500,1500,3500),.001);}));
    double TotalTurn=0,PreviousAngle=FMath::Atan2(Route[0].Y-6500,Route[0].X-6500);
    double MaxGrade=0;
    for (int32 I=1; I<Route.Num(); ++I)
    {
        const double Span=FVector::Dist2D(Route[I],Route[I-1]);
        TestTrue(TEXT("Road sampling no wider than 300cm"),Span>0 && Span<=300);
        MaxGrade=FMath::Max(MaxGrade,FMath::Abs(Route[I].Z-Route[I-1].Z)/Span);
        TestTrue(TEXT("Ascent never falls or exceeds the plateau"),Route[I].Z>=Route[I-1].Z && Route[I].Z<=3500);
        const double Angle=FMath::Atan2(Route[I].Y-6500,Route[I].X-6500);
        double Change=Angle-PreviousAngle;
        if (Change<-UE_PI) Change+=2*UE_PI;
        if (Change>UE_PI) Change-=2*UE_PI;
        TotalTurn+=Change; PreviousAngle=Angle;
    }
    TestTrue(TEXT("Approximately one complete ascending turn"),FMath::IsNearlyEqual(TotalTurn,2.0*UE_PI,.01));
    TestTrue(TEXT("Centreline grade below 14 percent"),MaxGrade<.14);
    FSettings Settings;
    Settings.Seed=12457;
    HearthRoyalHill::AddToTerrain(Settings);
    const auto First=Settings.Roads.Last();
    HearthRoyalHill::AddToTerrain(Settings);
    TestEqual(TEXT("Repeated append never duplicates owned road"),Settings.Roads.Num(),1);
    TestEqual(TEXT("Royal road width matches parent skip/filter contract"),First.Width,600.f);
    TestTrue(TEXT("Flat shoulders are provided in addition to 600cm carriageway"),First.ShoulderWidth>=450.f);
    TestEqual(TEXT("Entire ascent remains one profile"),First.Nodes.Num(),Route.Num());
    FSettings SeededLegacy; SeededLegacy.Seed=12457;
    TestEqual(TEXT("Custom seed also matches legacy toe"),First.Nodes[0].Elevation,HeightAt(FVector2D(6500,-1900),SeededLegacy));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthRoyalHillGridRoadTest,
    "ThreeHearths.RoyalHill.ActualGridCarriagewayGrades",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthRoyalHillGridRoadTest::RunTest(const FString&)
{
    using namespace HearthOrganicTerrain;
    using namespace HearthRoyalHillTests;
    const auto Route=HearthRoyalHill::AscentRoute();
    auto Settings=WithLegacyFeatures();
    // Append a stale low road AFTER AddToTerrain, crossing the elevated ascent.
    // Its ordering must not flatten or cut away the protected road profile.
    const auto Mid=Route[90];
    FRoadCenterline Stale;
    Stale.Width=600; Stale.Transition=500;
    Stale.Nodes={{FVector2D(Mid.X-2500,Mid.Y),80},{FVector2D(Mid.X+2500,Mid.Y),80}};
    Settings.Roads.Add(Stale);
    double MaxLong=0,MaxCross=0,MaxHeightError=0;
    // Coarse 300cm mesh AND refined 150cm mesh: passing only analytic samples
    // could conceal cliff interpolation at the edge of a narrow graded strip.
    for (int32 Quads : {100,200})
    {
        Settings.GridQuadsX=Quads; Settings.GridQuadsY=Quads;
        FGrid Grid;
        if (!TestTrue(TEXT("Actual terrain grid builds"),GenerateGrid(Settings,Grid))) return false;
        for (int32 I=1; I<Route.Num(); ++I)
        {
            const FVector A=Route[I-1],B=Route[I];
            const FVector Direction=(B-A).GetSafeNormal2D();
            const FVector2D Along(Direction.X,Direction.Y),Across(-Direction.Y,Direction.X);
            const int32 Samples=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/50));
            for (int32 J=0; J<=Samples; ++J)
            {
                const FVector P=FMath::Lerp(A,B,double(J)/Samples);
                for (double Offset : {-300.0,-150.0,0.0,150.0,300.0})
                {
                    const FVector2D XY=FVector2D(P.X,P.Y)+Across*Offset;
                    const double H=MeshHeight(Grid,Settings,XY);
                    const double Long=FMath::Abs(MeshHeight(Grid,Settings,XY+Along*25)-MeshHeight(Grid,Settings,XY-Along*25))/50;
                    const double Cross=FMath::Abs(MeshHeight(Grid,Settings,XY+Across*25)-MeshHeight(Grid,Settings,XY-Across*25))/50;
                    MaxLong=FMath::Max(MaxLong,Long); MaxCross=FMath::Max(MaxCross,Cross);
                    MaxHeightError=FMath::Max(MaxHeightError,FMath::Abs(H-P.Z));
                    TestTrue(TEXT("No discontinuous terrain cracks at sub-centimetre samples"),
                        FMath::Abs(MeshHeight(Grid,Settings,XY+Along*.1)-MeshHeight(Grid,Settings,XY-Along*.1))<.1);
                }
            }
        }
        // Grid normals follow finite differences of the stored vertex heights,
        // including one-sided edge samples. They are not replaced by UpVector.
        for (int32 Y=0; Y<Grid.VertexRows; Y+=7) for (int32 X=0; X<Grid.VertexColumns; X+=7)
        {
            const auto At=[&](int32 C,int32 R)->const FVector&{return Grid.Vertices[R*Grid.VertexColumns+C];};
            const auto L=At(FMath::Max(0,X-1),Y),R=At(FMath::Min(Grid.VertexColumns-1,X+1),Y);
            const auto D=At(X,FMath::Max(0,Y-1)),U=At(X,FMath::Min(Grid.VertexRows-1,Y+1));
            const FVector TX(R.X-L.X,0,R.Z-L.Z),TY(0,U.Y-D.Y,U.Z-D.Z);
            const FVector Normal=Grid.Normals[Y*Grid.VertexColumns+X];
            TestTrue(TEXT("Normal tangent to sampled heightfield"),FMath::Abs(FVector::DotProduct(Normal,TX.GetSafeNormal()))<.001
                && FMath::Abs(FVector::DotProduct(Normal,TY.GetSafeNormal()))<.001 && Normal.Z>0);
        }
    }
    AddInfo(FString::Printf(TEXT("Actual grid road grades: longitudinal %.4f%%, transverse %.4f%%; max height error %.3fcm"),MaxLong*100,MaxCross*100,MaxHeightError));
    TestTrue(TEXT("Entire 600cm actual mesh lane: longitudinal grade below 14 percent"),MaxLong<.14);
    TestTrue(TEXT("Entire 600cm actual mesh lane: transverse grade below 12 percent"),MaxCross<.12);
    TestTrue(TEXT("Both grid resolutions track the planned carriageway, not the raw hillside"),MaxHeightError<15);
    for (const auto& Pad : Settings.FlattenZones)
        TestTrue(TEXT("Existing home pad centres remain at saved natural height"),FMath::Abs(HeightAt(Pad.Center,Settings)-Pad.Elevation)<.1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthRoyalHillCityRoadsTest,
    "ThreeHearths.RoyalHill.Version4FootRingAndLegacyRoads",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthRoyalHillCityRoadsTest::RunTest(const FString&)
{
    const auto V3=HearthCityPlan::BuildVersion3();
    const auto V4=HearthCityPlan::BuildForVersion(4);
    TestEqual(TEXT("Version4 dispatch is explicit"),V4.LayoutVersion,4);
    TestEqual(TEXT("Version3 remains version3"),HearthCityPlan::BuildForVersion(3).LayoutVersion,3);
    TestTrue(TEXT("Map/world XY extent unchanged"),V3.MapMin==V4.MapMin && V3.MapMax==V4.MapMax);
    const FVector C(6500,6500,0);
    for (const auto& Old : V3.Roads)
    {
        if(FVector::Dist2D(Old.A,FVector(3000,450,0))<1 && FVector::Dist2D(Old.B,FVector(6500,1500,0))<1) continue;
        TestTrue(TEXT("Old southwest road geometry and width preserved exactly"),V4.Roads.ContainsByPredicate([&](const auto& R)
            {return R.A==Old.A && R.B==Old.B && R.Width==Old.Width;}));
    }
    int32 Royal=0,Ring=0;
    double MinimumOuterRadius=TNumericLimits<double>::Max();
    double MaximumOuterRadius=0.0;
    for (const auto& Road : V4.Roads)
    {
        if (Road.Width==HearthRoyalHill::RoadWidth) ++Royal;
        else
        {
            TestTrue(TEXT("Ordinary roads are below parent skip threshold"),Road.Width<HearthRoyalHill::RoadWidth);
            if (FMath::IsNearlyEqual(Road.Width,480.f,.1f))
            {
                ++Ring;
                const double Radius=HearthRoyalHillTests::RadiusToSegment(C,Road.A,Road.B);
                MinimumOuterRadius=FMath::Min(MinimumOuterRadius,Radius);
                MaximumOuterRadius=FMath::Max(MaximumOuterRadius,Radius);
                TestTrue(TEXT("Irregular civic road stays beyond the royal hill"),Radius>9200.0);
            }
        }
        TestFalse(TEXT("Former direct uphill chord is retired in v4"),Road.A.Equals(FVector(3000,450,8)) && Road.B.Equals(FVector(6500,1500,8)));
    }
    TestEqual(TEXT("All ascent segments exposed to road drawing"),Royal,HearthRoyalHill::AscentRoute().Num()-1);
    TestTrue(TEXT("Authored outer civic road is densely sampled"),Ring>200);
    TestTrue(TEXT("Outer civic road has authored radius variation"),MaximumOuterRadius-MinimumOuterRadius>500.0);
    TestTrue(TEXT("Legacy south toe remains the outer-road junction"),V4.Roads.ContainsByPredicate([](const auto& Road)
        {return Road.Width==480.f && (FVector::Dist2D(Road.A,FVector(6500,-4000,0))<.01f || FVector::Dist2D(Road.B,FVector(6500,-4000,0))<.01f);}));
    for (const auto& Road : V4.Roads)
        TestTrue(TEXT("All V4 road endpoints stay inside the 300m plan"),
            Road.A.X>=V4.MapMin.X && Road.A.X<=V4.MapMax.X && Road.A.Y>=V4.MapMin.Y && Road.A.Y<=V4.MapMax.Y
            && Road.B.X>=V4.MapMin.X && Road.B.X<=V4.MapMax.X && Road.B.Y>=V4.MapMin.Y && Road.B.Y<=V4.MapMax.Y);
    TestTrue(TEXT("Castle city marker uses accessible unloading point"),V4.Landmarks.ContainsByPredicate([](const auto& L)
        {return L.Id==TEXT("main_keep") && L.Position.Equals(FVector(6500,6500,3500)) && L.Approach.Equals(HearthRoyalHill::DeliveryApproach());}));
    return true;
}

#endif
