#pragma once

#include "Engine/Scene.h"

namespace HearthAincradViewGrade
{
    inline void Apply(FPostProcessSettings& Settings)
    {
        Settings.bOverride_AutoExposureMethod = true;
        Settings.AutoExposureMethod = AEM_Manual;
        Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        Settings.AutoExposureApplyPhysicalCameraExposure = false;
        Settings.bOverride_AutoExposureBias = true;
        Settings.AutoExposureBias = 1.f;
        Settings.bOverride_BloomIntensity = true;
        Settings.BloomIntensity = .15f;
        Settings.bOverride_MotionBlurAmount = true;
        Settings.MotionBlurAmount = 0.f;
        Settings.bOverride_VignetteIntensity = true;
        Settings.VignetteIntensity = 0.f;
        Settings.bOverride_AmbientOcclusionIntensity = true;
        Settings.AmbientOcclusionIntensity = .4f;
    }
}
