#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"
#include "HearthHorseCart.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightVisualDirectionTest,"ThreeHearths.Medieval.FreightVisualDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightVisualDirectionTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("visual world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Village=World->SpawnActor<AHearthVillage>();
    if(!TestNotNull(TEXT("village"),Village)) return false;
    auto* Cart=World->SpawnActor<AHearthHorseCart>();
    if(!TestNotNull(TEXT("cart"),Cart) || !TestTrue(TEXT("authored cart loads"),Cart->Configure(Village))) return false;
    TArray<UStaticMeshComponent*> Parts;Cart->GetComponents(Parts);
    UStaticMeshComponent* Wheel=nullptr;
    for(auto* Part:Parts) if(Part->ComponentHasTag(TEXT("cart_wheel"))) { Wheel=Part;break; }
    if(!TestNotNull(TEXT("authored wheel"),Wheel)) return false;
    FHearthFreightVisualState State;State.bEnabled=true;State.bMoving=true;State.CargoType=4;State.CargoQuantity=6;
    State.CartPosition=FVector::ZeroVector;State.CartYaw=0;State.WheelDistanceCm=0;
    Cart->ApplyState(State);
    const FQuat Rest=Wheel->GetRelativeRotation().Quaternion();
    State.CartPosition.X=30;State.WheelDistanceCm=30;Cart->ApplyState(State);
    const FQuat Forward=Wheel->GetRelativeRotation().Quaternion();
    const FQuat ForwardDelta=Rest.Inverse()*Forward;
    TestTrue(TEXT("actual forward motion rolls forwards"),ForwardDelta.X<-.01);
    State.CartPosition.X=20;State.WheelDistanceCm=40;Cart->ApplyState(State);
    const FQuat Reverse=Wheel->GetRelativeRotation().Quaternion();
    const FQuat ReverseDelta=Forward.Inverse()*Reverse;
    TestTrue(TEXT("backing reverses wheel rotation despite increasing odometer"),ReverseDelta.X>.01);
    State.bMoving=false;State.ElapsedSeconds+=120;Cart->ApplyState(State);
    TestTrue(TEXT("waiting does not rotate the wheel"),Reverse.Equals(Wheel->GetRelativeRotation().Quaternion(),.0001));
    int32 VisibleBeamLayers=0;
    for(auto* Part:Parts) if(Part->ComponentHasTag(TEXT("cargo_beams")) && Part->IsVisible()) ++VisibleBeamLayers;
    TestEqual(TEXT("six real beam units retain both authored layers"),VisibleBeamLayers,12);
    State.CargoQuantity=0;Cart->ApplyState(State);
    for(auto* Part:Parts) if(Part->ComponentHasTag(TEXT("cargo_beams"))) TestFalse(TEXT("unloaded beams are hidden"),Part->IsVisible());
    // A freshly loaded actor establishes its cosmetic phase from the saved
    // odometer, then still reverses the first actual backward movement.
    auto* Cold=World->SpawnActor<AHearthHorseCart>();
    if(!TestNotNull(TEXT("cold cart"),Cold) || !TestTrue(TEXT("cold cart loads"),Cold->Configure(Village))) return false;
    Cold->ApplyState(State);TArray<UStaticMeshComponent*> ColdParts;Cold->GetComponents(ColdParts);
    UStaticMeshComponent* ColdWheel=nullptr;
    for(auto* Part:ColdParts) if(Part->ComponentHasTag(TEXT("cart_wheel"))) { ColdWheel=Part;break; }
    if(!TestNotNull(TEXT("cold wheel"),ColdWheel)) return false;
    const FQuat ColdBefore=ColdWheel->GetRelativeRotation().Quaternion();
    State.bMoving=true;State.CartPosition.X-=10;State.WheelDistanceCm+=10;Cold->ApplyState(State);
    TestTrue(TEXT("first cold-restored backward movement rolls backwards"),(ColdBefore.Inverse()*ColdWheel->GetRelativeRotation().Quaternion()).X>.01);
    return true;
}
#endif
