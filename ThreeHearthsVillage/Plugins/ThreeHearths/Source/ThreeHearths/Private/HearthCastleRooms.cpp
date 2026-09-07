#include "HearthCastleRooms.h"

namespace HearthCastleRooms
{
    TArray<FHearthCastleRoom> BuildV2()
    {
        TArray<FHearthCastleRoom> Rooms;
        Rooms.Reserve(8);
        Rooms.Add({TEXT("throne_hall"), TEXT("厅堂"), TEXT("council_and_public_hearing"), FVector(0.f, 650.f, 0.f), FVector(0.f, -250.f, 0.f), FVector2D(1400.f, 1000.f), 0, false, true});
        Rooms.Add({TEXT("royal_treasury"), TEXT("库房"), TEXT("stored_goods_and_tax_records"), FVector(0.f, 1280.f, 0.f), FVector(0.f, 980.f, 0.f), FVector2D(1200.f, 600.f), 0, false, true});
        Rooms.Add({TEXT("west_side_wing"), TEXT("西侧翼"), TEXT("guards_and_crafts"), FVector(-2200.f, 700.f, 0.f), FVector(-1400.f, -190.f, 0.f), FVector2D(1600.f, 1800.f), 0, false, true});
        Rooms.Add({TEXT("east_side_wing"), TEXT("东侧翼"), TEXT("service_and_residence"), FVector(2200.f, 700.f, 0.f), FVector(1400.f, -190.f, 0.f), FVector2D(1600.f, 1800.f), 0, false, true});
        Rooms.Add({TEXT("upper_archive"), TEXT("上层档案室"), TEXT("archive_and_planning"), FVector(0.f, 650.f, 700.f), FVector(0.f, -250.f, 700.f), FVector2D(1400.f, 1000.f), 1, false, true});
        Rooms.Add({TEXT("gatehouse"), TEXT("南门塔"), TEXT("entry_control"), FVector(0.f, -2650.f, 0.f), FVector(0.f, -3130.f, 0.f), FVector2D(1800.f, 700.f), 0, false, true});
        Rooms.Add({TEXT("inner_courtyard"), TEXT("内庭院"), TEXT("circulation_and_garden"), FVector(0.f, -1200.f, 0.f), FVector(0.f, -1900.f, 0.f), FVector2D(2600.f, 1500.f), 0, true, true});
        Rooms.Add({TEXT("service_yard"), TEXT("后勤院"), TEXT("deliveries_and_material_staging"), FVector(0.f, 2350.f, 0.f), FVector(0.f, 2700.f, 0.f), FVector2D(2600.f, 700.f), 0, true, true});
        return Rooms;
    }
}
