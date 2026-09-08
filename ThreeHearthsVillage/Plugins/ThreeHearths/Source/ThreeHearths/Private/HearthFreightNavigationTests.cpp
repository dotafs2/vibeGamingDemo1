#if WITH_DEV_AUTOMATION_TESTS
#include "HearthFreightNavigation.h"
#include "Misc/AutomationTest.h"

namespace
{
    using HearthFreightNavigation::FPose;

    float YawDelta(float A, float B)
    {
        return FMath::Abs(FMath::UnwindDegrees(B - A));
    }

    bool GeometryIsContinuous(const TArray<FPose>& Route)
    {
        if (Route.IsEmpty()) return false;
        for (int32 I = 1; I < Route.Num(); ++I)
        {
            const float Distance = FVector::Dist2D(Route[I - 1].Position, Route[I].Position);
            if (Distance <= .01f || Distance > 50.01f) return false;
            if (FMath::DegreesToRadians(YawDelta(Route[I - 1].Yaw, Route[I].Yaw)) > Distance / 300.f + .001f) return false;
            const float MidYaw=Route[I-1].Yaw+FMath::FindDeltaAngleDegrees(Route[I-1].Yaw,Route[I].Yaw)*.5f;
            if(FVector::DotProduct((Route[I].Position-Route[I-1].Position).GetSafeNormal2D(),FRotator(0,MidYaw,0).Vector())<.999f) return false;
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightNavigationStraightTest,
    "ThreeHearths.FreightNavigation.StraightForward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightNavigationStraightTest::RunTest(const FString&)
{
    const FPose Start{FVector::ZeroVector, 0.f};
    TArray<FPose> Route;
    auto Clear=[](const FPose&) { return true; };
    TestTrue(TEXT("straight route is found"), HearthFreightNavigation::Plan(Start, FVector(900.f,0.f,0.f), Clear, Route));
    TestTrue(TEXT("route is continuous"), GeometryIsContinuous(Route));
    TestTrue(TEXT("first pose preserves start yaw"), Route.Num()>0 && FMath::IsNearlyEqual(Route[0].Yaw, Start.Yaw));
    TestTrue(TEXT("route reaches the goal tolerance"), Route.Num()>0 && FVector::Dist2D(Route.Last().Position,FVector(900.f,0.f,0.f))<=150.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightNavigationTurnTest,
    "ThreeHearths.FreightNavigation.ForwardUTurn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightNavigationTurnTest::RunTest(const FString&)
{
    TArray<FPose> Route;
    auto Clear=[](const FPose&) { return true; };
    TestTrue(TEXT("an open area permits a forward-only U-turn"), HearthFreightNavigation::Plan(
        FPose{FVector::ZeroVector,0.f}, FVector(-700.f,0.f,0.f), Clear, Route, 6000));
    TestTrue(TEXT("U-turn has no reverse or teleport step"), GeometryIsContinuous(Route));
    TestTrue(TEXT("U-turn reaches its rearward goal"), Route.Num()>0 && FVector::Dist2D(Route.Last().Position,FVector(-700.f,0.f,0.f))<=150.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightNavigationObstacleTest,
    "ThreeHearths.FreightNavigation.ObstacleDetour",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightNavigationObstacleTest::RunTest(const FString&)
{
    TArray<FPose> Route;
    auto Clear=[](const FPose& Pose)
    {
        // Leave enough approach distance for a forward-only vehicle with
        // minimum radius 350 cm to get one side outside the obstacle.
        return !(Pose.Position.X>450.f && Pose.Position.X<800.f
            && FMath::Abs(Pose.Position.Y)<240.f);
    };
    TestTrue(TEXT("planner detours around a square obstacle"), HearthFreightNavigation::Plan(
        FPose{FVector::ZeroVector,0.f}, FVector(1000.f,0.f,0.f), Clear, Route));
    bool bAvoids=true;
    for(const FPose& Pose:Route) bAvoids=bAvoids && Clear(Pose);
    TestTrue(TEXT("every detour sample is clear"), bAvoids);
    TestTrue(TEXT("detour remains continuous"), GeometryIsContinuous(Route));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightNavigationFailureTest,
    "ThreeHearths.FreightNavigation.BoundedFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightNavigationFailureTest::RunTest(const FString&)
{
    TArray<FPose> Route;
    auto BlockedStart=[](const FPose&) { return false; };
    TestFalse(TEXT("blocked start fails without fallback"), HearthFreightNavigation::Plan(
        FPose{FVector::ZeroVector,0.f}, FVector(1000.f,0.f,0.f), BlockedStart, Route));
    auto Clear=[](const FPose&) { return true; };
    TestFalse(TEXT("one expansion cannot claim an unreachable plan"), HearthFreightNavigation::Plan(
        FPose{FVector::ZeroVector,0.f}, FVector(1000.f,0.f,0.f), Clear, Route, 1));
    TestTrue(TEXT("failed plans leave no stale output"), Route.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightNavigationCallbackTest,
    "ThreeHearths.FreightNavigation.CallbackAndCurvature",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightNavigationCallbackTest::RunTest(const FString&)
{
    TArray<FPose> Route;
    int32 Calls=0;
    auto Clear=[&Calls](const FPose&) { ++Calls; return true; };
    TestTrue(TEXT("callback-backed route succeeds"), HearthFreightNavigation::Plan(
        FPose{FVector::ZeroVector,0.f}, FVector(1200.f,0.f,0.f), Clear, Route));
    TestTrue(TEXT("callback sees start and every candidate sample"), Calls>Route.Num());
    TestTrue(TEXT("all output samples are continuous"), GeometryIsContinuous(Route));
    return true;
}
#endif
