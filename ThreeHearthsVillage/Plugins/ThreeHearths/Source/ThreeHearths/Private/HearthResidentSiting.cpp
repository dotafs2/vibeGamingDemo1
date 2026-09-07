#include "HearthResidentSiting.h"

namespace HearthResidentSitingDetail
{
    constexpr float MaxPenalty = 30.f;

    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    float SafeDistance2D(const FVector& A, const FVector& B)
    {
        const float DX = FMath::Clamp(A.X - B.X, -1000000.f, 1000000.f);
        const float DY = FMath::Clamp(A.Y - B.Y, -1000000.f, 1000000.f);
        return FMath::Sqrt(DX * DX + DY * DY);
    }

    float NearPenalty(const FVector& Candidate, const TArray<FVector>& Points, float Radius, bool& bFound)
    {
        bFound = false;
        float Best = 1.f;
        const int32 Limit = FMath::Min(Points.Num(), 64);
        for (int32 Index = 0; Index < Limit; ++Index)
        {
            if (!IsFiniteVector(Points[Index])) continue;
            bFound = true;
            Best = FMath::Min(Best, FMath::Clamp(SafeDistance2D(Candidate, Points[Index]) / Radius, 0.f, 1.f));
        }
        return Best;
    }

    float SingleNearPenalty(const FVector& Candidate, const FVector& Point, float Radius, bool& bFound)
    {
        bFound = IsFiniteVector(Point);
        return bFound ? FMath::Clamp(SafeDistance2D(Candidate, Point) / Radius, 0.f, 1.f) : 1.f;
    }

    bool IsCraftRole(const FString& Role)
    {
        return Role == TEXT("木匠") || Role == TEXT("石匠") || Role == TEXT("陶工")
            || Role == TEXT("铁匠") || Role == TEXT("织工") || Role.Contains(TEXT("农"));
    }

    bool IsSociable(const FHearthResidentSitingInput& Input)
    {
        return Input.Personality.Contains(TEXT("热心")) || Input.Personality.Contains(TEXT("健谈"))
            || Input.Personality.Contains(TEXT("温和")) || Input.Personality.Contains(TEXT("邻居"))
            || Input.Personality.Contains(TEXT("友情")) || Input.Goal.Contains(TEXT("邻里"))
            || Input.Goal.Contains(TEXT("招待"));
    }

    bool IsQuiet(const FHearthResidentSitingInput& Input)
    {
        return Input.Personality.Contains(TEXT("内向")) || Input.Personality.Contains(TEXT("安静"))
            || Input.Personality.Contains(TEXT("树林")) || Input.Personality.Contains(TEXT("私"))
            || Input.Goal.Contains(TEXT("安静")) || Input.Goal.Contains(TEXT("私密"));
    }

    void AddReason(FString& Reason, const TCHAR* Text)
    {
        if (Reason.IsEmpty()) Reason = Text;
        else Reason += FString::Printf(TEXT("；%s"), Text);
        if (Reason.Len() > 180) Reason.LeftInline(180);
    }
}

FHearthResidentSitingResult HearthResidentSiting::Evaluate(const FHearthResidentSitingInput& Input, const FVector& Candidate)
{
    using namespace HearthResidentSitingDetail;
    FHearthResidentSitingResult Result;
    if (!IsFiniteVector(Candidate))
    {
        Result.Penalty = MaxPenalty;
        Result.Reason = TEXT("候选坐标无效，偏好降级。");
        return Result;
    }

    float Score = 0.f;
    FString Reason;
    bool bFound = false;
    const float HomePenalty = SingleNearPenalty(Candidate, IsFiniteVector(Input.Home) ? Input.Home : Input.Current, 1800.f, bFound);
    Score += HomePenalty * 2.f;

    if (IsCraftRole(Input.Role))
    {
        const float WorkPenalty = NearPenalty(Candidate, Input.Workpoints, 1100.f, bFound);
        Score += WorkPenalty * 12.f;
        AddReason(Reason, bFound ? TEXT("靠近自己的工作点") : TEXT("没有有效工作点，采用通用偏好"));
    }
    else if (Input.Role == TEXT("商人"))
    {
        const float MarketPenalty = SingleNearPenalty(Candidate, Input.Market, 1300.f, bFound);
        Score += MarketPenalty * 12.f;
        AddReason(Reason, bFound ? TEXT("靠近市场和街面") : TEXT("没有有效市场点，采用通用偏好"));
    }

    if (IsSociable(Input))
    {
        const float FriendPenalty = NearPenalty(Candidate, Input.Friends, 1050.f, bFound);
        Score += FriendPenalty * 9.f;
        AddReason(Reason, bFound ? TEXT("偏好靠近真实朋友") : TEXT("偏好邻里交往"));
    }

    if (IsQuiet(Input))
    {
        const float PublicPenalty = SingleNearPenalty(Candidate, Input.Market, 1300.f, bFound);
        Score += (1.f - PublicPenalty) * 7.f;
        const float ReachPenalty = SingleNearPenalty(Candidate, IsFiniteVector(Input.Current) ? Input.Current : Input.Home, 2400.f, bFound);
        Score += ReachPenalty * 3.f;
        AddReason(Reason, TEXT("偏好安静私密空间"));
    }

    if (Input.bKing)
    {
        const float PrestigePenalty = SingleNearPenalty(Candidate, Input.Market, 1800.f, bFound);
        Score += PrestigePenalty * 4.f;
        AddReason(Reason, TEXT("保留靠近公共中心的王者愿望"));
    }

    // A future court-order story is not a real business location. Only an
    // explicit frontage/market preference changes present-day siting.
    if (Input.Goal.Contains(TEXT("市场")) || Input.Goal.Contains(TEXT("临街")))
    {
        const float MarketPenalty = SingleNearPenalty(Candidate, Input.Market, 1600.f, bFound);
        Score += MarketPenalty * 3.f;
        AddReason(Reason, TEXT("目标需要接近市场"));
    }

    Result.Penalty = FMath::Clamp(FMath::IsFinite(Score) ? Score : MaxPenalty, 0.f, MaxPenalty);
    Result.Reason = Reason.IsEmpty() ? TEXT("按当前与家的距离保留普通偏好") : Reason;
    Result.Reason.LeftInline(180);
    return Result;
}
