#if WITH_DEV_AUTOMATION_TESTS

#include "HearthAincradEcology.h"
#include "HearthRoyalHill.h"
#include "HearthCityPlan.h"
#include "Misc/AutomationTest.h"

namespace HearthAincradEcologyTestDetail
{
    HearthAincradEcology::FInput Input()
    {
        HearthAincradEcology::FInput Input;
        for(const auto& Road:HearthCityPlan::BuildVersion4().Roads)
        {
            if(Road.Width>=HearthRoyalHill::RoadWidth) continue;
            HearthOrganicTerrain::FRoadCenterline Lane; Lane.Width=Road.Width; Lane.Transition=360;
            Lane.Nodes.Add({FVector2D(Road.A.X,Road.A.Y),0});
            Lane.Nodes.Add({FVector2D(Road.B.X,Road.B.Y),0});
            Input.Terrain.Roads.Add(Lane);
        }
        HearthRoyalHill::AddToTerrain(Input.Terrain);
        for(const FVector2D& P:{FVector2D(2763,-581),FVector2D(4566,-1486),FVector2D(4321,-3025),
            FVector2D(4626,-40),FVector2D(-1639,443),FVector2D(481,-39),FVector2D(-1616,-4427),
            FVector2D(-4880,-3159),FVector2D(-6036,-1396),FVector2D(-4379,-4744)})
            Input.Clearings.Add({P,FVector2D(1100,1100),0});
        return Input;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradEcologyDeterminismTest,
    "ThreeHearths.AincradEcology.DeterministicBoundedWoodland",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthAincradEcologyDeterminismTest::RunTest(const FString& Parameters)
{
    const auto Input=HearthAincradEcologyTestDetail::Input();
    const auto A=HearthAincradEcology::BuildPlan(Input),B=HearthAincradEcology::BuildPlan(Input);
    TestEqual(TEXT("Same terrain seed reproduces all scenery"),A.Placements.Num(),B.Placements.Num());
    TestTrue(TEXT("Full but bounded tree population"),A.Trees>=120 && A.Trees<=220);
    TestTrue(TEXT("Understory stays bounded"),A.Understory>100 && A.Understory<=408);
    TestTrue(TEXT("Candidate retries have a fixed budget"),A.Candidates<=608*28);
    int32 North=0,West=0,East=0,SouthTrees=0;
    for(int32 I=0;I<A.Placements.Num();++I)
    {
        const auto& P=A.Placements[I];
        if(B.Placements.IsValidIndex(I))
        {
            const auto& Q=B.Placements[I];
            TestTrue(TEXT("Transform and native species are deterministic"),P.XY==Q.XY && P.Yaw==Q.Yaw && P.Scale==Q.Scale && P.Species==Q.Species);
        }
        if(P.Species<2)
        {
            if(P.XY.Y>15000) ++North;
            if(P.XY.X<-6000) ++West;
            if(P.XY.X>18000) ++East;
            if(P.Region>=8) ++SouthTrees;
        }
        TestNotNull(TEXT("Every species has a discovered native asset"),HearthAincradEcology::AssetPath(P.Species));
    }
    TestTrue(TEXT("Woods extend beyond old 130m scatter on three sides"),North>30 && West>30 && East>0);
    TestTrue(TEXT("Southern meadow has only sparse tree groups"),SouthTrees<=12);
    auto Different=Input; Different.Terrain.Seed+=17;
    const auto C=HearthAincradEcology::BuildPlan(Different);
    TestTrue(TEXT("A different world seed changes the arrangement"),A.Placements.Num()>0 && C.Placements.Num()>0 && A.Placements[0].XY!=C.Placements[0].XY);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradEcologyClearanceTest,
    "ThreeHearths.AincradEcology.SavedClearingsAndFullAscent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthAincradEcologyClearanceTest::RunTest(const FString& Parameters)
{
    auto Input=HearthAincradEcologyTestDetail::Input();
    // An additional saved work plot and a rotated pad must cut a genuine gap
    // out of a woodland, independent of the terrain seed and foliage species.
    Input.Clearings.Add({FVector2D(-6500,7200),FVector2D(900,650),31});
    HearthOrganicTerrain::FFlattenZone Pad; Pad.Center=FVector2D(5500,19800);
    Pad.HalfSize=FVector2D(650,400); Pad.YawDegrees=47;
    Input.Terrain.FlattenZones.Add(Pad);
    const auto Plan=HearthAincradEcology::BuildPlan(Input);
    for(const auto& P:Plan.Placements)
    {
        const auto& Bounds=Input.Terrain.Bounds;
        TestTrue(TEXT("Full native footprint stays inside terrain"),P.XY.X-P.Radius>=Bounds.Min.X && P.XY.X+P.Radius<=Bounds.Max.X
            && P.XY.Y-P.Radius>=Bounds.Min.Y && P.XY.Y+P.Radius<=Bounds.Max.Y);
        TestTrue(TEXT("Castle plateau remains clear"),(P.XY-HearthRoyalHill::Center()).Size()>HearthRoyalHill::PlateauRadius+P.Radius);
        const auto CheckRect=[&](const FVector2D& Center,const FVector2D& Half,float Yaw)
        {
            const FVector2D Q=(P.XY-Center).GetRotated(-Yaw);
            const double DX=FMath::Max(0.0,FMath::Abs(Q.X)-Half.X),DY=FMath::Max(0.0,FMath::Abs(Q.Y)-Half.Y);
            TestTrue(TEXT("Whole canopy clears existing/reserved plot"),DX*DX+DY*DY>P.Radius*P.Radius);
        };
        CheckRect(HearthRoyalHill::Center(),FVector2D(5025,5025),0);
        for(const auto& C:Input.Clearings) CheckRect(C.Center,C.HalfSize,C.Yaw);
        for(const auto& Z:Input.Terrain.FlattenZones) CheckRect(Z.Center,Z.HalfSize,Z.YawDegrees);
        for(const auto& Road:Input.Terrain.Roads)
        {
            const double Half=Road.bRoyalHillRoad?Road.Width*.5+Road.ShoulderWidth+Road.Transition:Road.Width+Road.Transition;
            for(int32 I=1;I<Road.Nodes.Num();++I)
            {
                const FVector Closest=FMath::ClosestPointOnSegment(FVector(P.XY,0),FVector(Road.Nodes[I-1].Position,0),FVector(Road.Nodes[I].Position,0));
                TestTrue(TEXT("All roads, shoulders and ascent segments stay free"),FVector::Dist2D(FVector(P.XY,0),Closest)>Half+P.Radius);
            }
        }
    }
    Input.Clearings.Add({FVector2D(6500,6500),FVector2D(20000,20000),0});
    TestEqual(TEXT("A fully occupied world yields no decoration"),HearthAincradEcology::BuildPlan(Input).Placements.Num(),0);
    return true;
}

#endif
