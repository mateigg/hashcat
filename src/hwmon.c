/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "dynloader.h"
#include "shared.h"
#include "folder.h"
#include "hwmon.h"

// general functions

static int get_adapters_num_adl (hashcat_ctx_t *hashcat_ctx, int *iNumberAdapters)
{
  if (hm_ADL_Adapter_NumberOfAdapters_Get (hashcat_ctx, iNumberAdapters) == -1) return -1;

  if (iNumberAdapters == NULL)
  {
    event_log_error (hashcat_ctx, "No ADL adapters found.");

    return -1;
  }

  return 0;
}

static int hm_get_adapter_index_nvapi (hashcat_ctx_t *hashcat_ctx, HM_ADAPTER_NVAPI *nvapiGPUHandle)
{
  NvU32 pGpuCount;

  if (hm_NvAPI_EnumPhysicalGPUs (hashcat_ctx, nvapiGPUHandle, &pGpuCount) == -1) return 0;

  if (pGpuCount == 0)
  {
    event_log_error (hashcat_ctx, "No NvAPI adapters found.");

    return 0;
  }

  return (pGpuCount);
}

static int hm_get_adapter_index_nvml (hashcat_ctx_t *hashcat_ctx, HM_ADAPTER_NVML *nvmlGPUHandle)
{
  unsigned int deviceCount = 0;

  hm_NVML_nvmlDeviceGetCount (hashcat_ctx, &deviceCount);

  if (deviceCount == 0)
  {
    event_log_error (hashcat_ctx, "No NVML adapters found.");

    return 0;
  }

  for (u32 i = 0; i < deviceCount; i++)
  {
    if (hm_NVML_nvmlDeviceGetHandleByIndex (hashcat_ctx, i, &nvmlGPUHandle[i]) == -1) break;

    // can be used to determine if the device by index matches the cuda device by index
    // char name[100]; memset (name, 0, sizeof (name));
    // hm_NVML_nvmlDeviceGetName (hashcat_ctx, nvGPUHandle[i], name, sizeof (name) - 1);
  }

  return (deviceCount);
}

int hm_get_threshold_slowdown_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].threshold_slowdown_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      int target = 0;

      if (hm_NVML_nvmlDeviceGetTemperatureThreshold (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, (unsigned int *) &target) == -1)
      {
        hwmon_ctx->hm_device[backend_device_idx].threshold_slowdown_get_supported = false;

        return -1;
      }

      return target;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        if (hwmon_ctx->hm_device[backend_device_idx].od_version == 5)
        {

        }
        else if (hwmon_ctx->hm_device[backend_device_idx].od_version == 6)
        {
          int CurrentValue = 0;
          int DefaultValue = 0;

          if (hm_ADL_Overdrive6_TargetTemperatureData_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, &CurrentValue, &DefaultValue) == -1)
          {
            hwmon_ctx->hm_device[backend_device_idx].threshold_slowdown_get_supported = false;

            return -1;
          }

          // the return value has never been tested since hm_ADL_Overdrive6_TargetTemperatureData_Get() never worked on any system. expect problems.

          return DefaultValue;
        }
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        int target = 0;

        if (hm_NVML_nvmlDeviceGetTemperatureThreshold (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, (unsigned int *) &target) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].threshold_slowdown_get_supported = false;

          return -1;
        }

        return target;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].threshold_slowdown_get_supported = false;

  return -1;
}

int hm_get_threshold_shutdown_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].threshold_shutdown_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      int target = 0;

      if (hm_NVML_nvmlDeviceGetTemperatureThreshold (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_TEMPERATURE_THRESHOLD_SHUTDOWN, (unsigned int *) &target) == -1)
      {
        hwmon_ctx->hm_device[backend_device_idx].threshold_shutdown_get_supported = false;

        return -1;
      }

      return target;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        if (hwmon_ctx->hm_device[backend_device_idx].od_version == 5)
        {

        }
        else if (hwmon_ctx->hm_device[backend_device_idx].od_version == 6)
        {

        }
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        int target = 0;

        if (hm_NVML_nvmlDeviceGetTemperatureThreshold (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_TEMPERATURE_THRESHOLD_SHUTDOWN, (unsigned int *) &target) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].threshold_shutdown_get_supported = false;

          return -1;
        }

        return target;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].threshold_shutdown_get_supported = false;

  return -1;
}

int hm_get_temperature_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].temperature_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      int temperature = 0;

      if (hm_NVML_nvmlDeviceGetTemperature (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_TEMPERATURE_GPU, (u32 *) &temperature) == -1)
      {
        hwmon_ctx->hm_device[backend_device_idx].temperature_get_supported = false;

        return -1;
      }

      return temperature;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        if (hwmon_ctx->hm_device[backend_device_idx].od_version == 5)
        {
          ADLTemperature Temperature;

          Temperature.iSize = sizeof (ADLTemperature);

          if (hm_ADL_Overdrive5_Temperature_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, 0, &Temperature) == -1)
          {
            hwmon_ctx->hm_device[backend_device_idx].temperature_get_supported = false;

            return -1;
          }

          return Temperature.iTemperature / 1000;
        }

        if (hwmon_ctx->hm_device[backend_device_idx].od_version == 6)
        {
          int Temperature = 0;

          if (hm_ADL_Overdrive6_Temperature_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, &Temperature) == -1)
          {
            hwmon_ctx->hm_device[backend_device_idx].temperature_get_supported = false;

            return -1;
          }

          return Temperature / 1000;
        }
      }

      if (hwmon_ctx->hm_sysfs)
      {
        int temperature = 0;

        if (hm_SYSFS_get_temperature_current (hashcat_ctx, backend_device_idx, &temperature) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].temperature_get_supported = false;

          return -1;
        }

        return temperature;
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        int temperature = 0;

        if (hm_NVML_nvmlDeviceGetTemperature (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_TEMPERATURE_GPU, (u32 *) &temperature) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].temperature_get_supported = false;

          return -1;
        }

        return temperature;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].temperature_get_supported = false;

  return -1;
}

int hm_get_fanpolicy_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].fanpolicy_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    return 1;
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        if (hwmon_ctx->hm_device[backend_device_idx].od_version == 5)
        {
          ADLFanSpeedValue lpFanSpeedValue;

          memset (&lpFanSpeedValue, 0, sizeof (lpFanSpeedValue));

          lpFanSpeedValue.iSize      = sizeof (lpFanSpeedValue);
          lpFanSpeedValue.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_PERCENT;

          if (hm_ADL_Overdrive5_FanSpeed_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, 0, &lpFanSpeedValue) == -1)
          {
            hwmon_ctx->hm_device[backend_device_idx].fanpolicy_get_supported = false;
            hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported  = false;

            return -1;
          }

          return (lpFanSpeedValue.iFanSpeed & ADL_DL_FANCTRL_FLAG_USER_DEFINED_SPEED) ? 0 : 1;
        }

        if (hwmon_ctx->hm_device[backend_device_idx].od_version == 6)
        {
          return 1;
        }
      }

      if (hwmon_ctx->hm_sysfs)
      {
        return 1;
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      return 1;
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].fanpolicy_get_supported = false;
  hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported  = false;

  return -1;
}

int hm_get_fanspeed_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      int speed = 0;

      if (hm_NVML_nvmlDeviceGetFanSpeed (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, (u32 *) &speed) == -1)
      {
        hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported = false;

        return -1;
      }

      return speed;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        if (hwmon_ctx->hm_device[backend_device_idx].od_version == 5)
        {
          ADLFanSpeedValue lpFanSpeedValue;

          memset (&lpFanSpeedValue, 0, sizeof (lpFanSpeedValue));

          lpFanSpeedValue.iSize      = sizeof (lpFanSpeedValue);
          lpFanSpeedValue.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_PERCENT;
          lpFanSpeedValue.iFlags     = ADL_DL_FANCTRL_FLAG_USER_DEFINED_SPEED;

          if (hm_ADL_Overdrive5_FanSpeed_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, 0, &lpFanSpeedValue) == -1)
          {
            hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported = false;

            return -1;
          }

          return lpFanSpeedValue.iFanSpeed;
        }

        if (hwmon_ctx->hm_device[backend_device_idx].od_version == 6)
        {
          ADLOD6FanSpeedInfo faninfo;

          memset (&faninfo, 0, sizeof (faninfo));

          if (hm_ADL_Overdrive6_FanSpeed_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, &faninfo) == -1)
          {
            hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported = false;

            return -1;
          }

          return faninfo.iFanSpeedPercent;
        }
      }

      if (hwmon_ctx->hm_sysfs)
      {
        int speed = 0;

        if (hm_SYSFS_get_fan_speed_current (hashcat_ctx, backend_device_idx, &speed) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported = false;

          return -1;
        }

        return speed;
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        int speed = 0;

        if (hm_NVML_nvmlDeviceGetFanSpeed (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, (u32 *) &speed) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported = false;

          return -1;
        }

        return speed;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].fanspeed_get_supported = false;

  return -1;
}

int hm_get_buslanes_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].buslanes_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      unsigned int currLinkWidth;

      if (hm_NVML_nvmlDeviceGetCurrPcieLinkWidth (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, &currLinkWidth) == -1)
      {
        hwmon_ctx->hm_device[backend_device_idx].buslanes_get_supported = false;

        return -1;
      }

      return currLinkWidth;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        ADLPMActivity PMActivity;

        PMActivity.iSize = sizeof (ADLPMActivity);

        if (hm_ADL_Overdrive_CurrentActivity_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, &PMActivity) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].buslanes_get_supported = false;

          return -1;
        }

        return PMActivity.iCurrentBusLanes;
      }

      if (hwmon_ctx->hm_sysfs)
      {
        int lanes;

        if (hm_SYSFS_get_pp_dpm_pcie (hashcat_ctx, backend_device_idx, &lanes) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].buslanes_get_supported = false;

          return -1;
        }

        return lanes;
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        unsigned int currLinkWidth;

        if (hm_NVML_nvmlDeviceGetCurrPcieLinkWidth (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, &currLinkWidth) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].buslanes_get_supported = false;

          return -1;
        }

        return currLinkWidth;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].buslanes_get_supported = false;

  return -1;
}

int hm_get_utilization_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].utilization_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      nvmlUtilization_t utilization;

      if (hm_NVML_nvmlDeviceGetUtilizationRates (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, &utilization) == -1)
      {
        hwmon_ctx->hm_device[backend_device_idx].utilization_get_supported = false;

        return -1;
      }

      return utilization.gpu;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        ADLPMActivity PMActivity;

        PMActivity.iSize = sizeof (ADLPMActivity);

        if (hm_ADL_Overdrive_CurrentActivity_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, &PMActivity) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].utilization_get_supported = false;

          return -1;
        }

        return PMActivity.iActivityPercent;
      }

      if (hwmon_ctx->hm_sysfs)
      {
        int util;

        if (hm_SYSFS_get_gpu_busy_percent (hashcat_ctx, backend_device_idx, &util) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].utilization_get_supported = false;

          return -1;
        }

        return util;
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        nvmlUtilization_t utilization;

        if (hm_NVML_nvmlDeviceGetUtilizationRates (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, &utilization) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].utilization_get_supported = false;

          return -1;
        }

        return utilization.gpu;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].utilization_get_supported = false;

  return -1;
}

int hm_get_memoryspeed_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].memoryspeed_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      unsigned int clockfreq;

      if (hm_NVML_nvmlDeviceGetClockInfo (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_CLOCK_MEM, &clockfreq) == -1)
      {
        hwmon_ctx->hm_device[backend_device_idx].memoryspeed_get_supported = false;

        return -1;
      }

      return clockfreq;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        ADLPMActivity PMActivity;

        PMActivity.iSize = sizeof (ADLPMActivity);

        if (hm_ADL_Overdrive_CurrentActivity_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, &PMActivity) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].memoryspeed_get_supported = false;

          return -1;
        }

        return PMActivity.iMemoryClock / 100;
      }

      if (hwmon_ctx->hm_sysfs)
      {
        int clockfreq;

        if (hm_SYSFS_get_pp_dpm_mclk (hashcat_ctx, backend_device_idx, &clockfreq) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].memoryspeed_get_supported = false;

          return -1;
        }

        return clockfreq;
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        unsigned int clockfreq;

        if (hm_NVML_nvmlDeviceGetClockInfo (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_CLOCK_MEM, &clockfreq) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].memoryspeed_get_supported = false;

          return -1;
        }

        return clockfreq;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].memoryspeed_get_supported = false;

  return -1;
}

int hm_get_corespeed_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].corespeed_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      unsigned int clockfreq;

      if (hm_NVML_nvmlDeviceGetClockInfo (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_CLOCK_SM, &clockfreq) == -1)
      {
        hwmon_ctx->hm_device[backend_device_idx].corespeed_get_supported = false;

        return -1;
      }

      return clockfreq;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
      if (hwmon_ctx->hm_adl)
      {
        ADLPMActivity PMActivity;

        PMActivity.iSize = sizeof (ADLPMActivity);

        if (hm_ADL_Overdrive_CurrentActivity_Get (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].adl, &PMActivity) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].corespeed_get_supported = false;

          return -1;
        }

        return PMActivity.iEngineClock / 100;
      }

      if (hwmon_ctx->hm_sysfs)
      {
        int clockfreq;

        if (hm_SYSFS_get_pp_dpm_sclk (hashcat_ctx, backend_device_idx, &clockfreq) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].corespeed_get_supported = false;

          return -1;
        }

        return clockfreq;
      }
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        unsigned int clockfreq;

        if (hm_NVML_nvmlDeviceGetClockInfo (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, NVML_CLOCK_SM, &clockfreq) == -1)
        {
          hwmon_ctx->hm_device[backend_device_idx].corespeed_get_supported = false;

          return -1;
        }

        return clockfreq;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].corespeed_get_supported = false;

  return -1;
}

int hm_get_throttle_with_devices_idx (hashcat_ctx_t *hashcat_ctx, const int backend_device_idx)
{
  hwmon_ctx_t   *hwmon_ctx   = hashcat_ctx->hwmon_ctx;
  backend_ctx_t *backend_ctx = hashcat_ctx->backend_ctx;

  if (hwmon_ctx->enabled == false) return -1;

  if (hwmon_ctx->hm_device[backend_device_idx].throttle_get_supported == false) return -1;

  if (backend_ctx->devices_param[backend_device_idx].is_cuda == true)
  {
    if (hwmon_ctx->hm_nvml)
    {
      /* this is triggered by mask generator, too. therefore useless
      unsigned long long clocksThrottleReasons = 0;
      unsigned long long supportedThrottleReasons = 0;

      if (hm_NVML_nvmlDeviceGetCurrentClocksThrottleReasons   (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, &clocksThrottleReasons)    == -1) return -1;
      if (hm_NVML_nvmlDeviceGetSupportedClocksThrottleReasons (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, &supportedThrottleReasons) == -1) return -1;

      clocksThrottleReasons &=  supportedThrottleReasons;
      clocksThrottleReasons &= ~nvmlClocksThrottleReasonGpuIdle;
      clocksThrottleReasons &= ~nvmlClocksThrottleReasonApplicationsClocksSetting;
      clocksThrottleReasons &= ~nvmlClocksThrottleReasonUnknown;

      if (backend_ctx->kernel_power_final)
      {
        clocksThrottleReasons &= ~nvmlClocksThrottleReasonHwSlowdown;
      }

      return (clocksThrottleReasons != nvmlClocksThrottleReasonNone);
      */
    }

    if (hwmon_ctx->hm_nvapi)
    {
      NV_GPU_PERF_POLICIES_INFO_PARAMS_V1   perfPolicies_info;
      NV_GPU_PERF_POLICIES_STATUS_PARAMS_V1 perfPolicies_status;

      memset (&perfPolicies_info,   0, sizeof (NV_GPU_PERF_POLICIES_INFO_PARAMS_V1));
      memset (&perfPolicies_status, 0, sizeof (NV_GPU_PERF_POLICIES_STATUS_PARAMS_V1));

      perfPolicies_info.version   = MAKE_NVAPI_VERSION (NV_GPU_PERF_POLICIES_INFO_PARAMS_V1, 1);
      perfPolicies_status.version = MAKE_NVAPI_VERSION (NV_GPU_PERF_POLICIES_STATUS_PARAMS_V1, 1);

      hm_NvAPI_GPU_GetPerfPoliciesInfo (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvapi, &perfPolicies_info);

      perfPolicies_status.info_value = perfPolicies_info.info_value;

      hm_NvAPI_GPU_GetPerfPoliciesStatus (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvapi, &perfPolicies_status);

      return perfPolicies_status.throttle & 2;
    }
  }

  if (backend_ctx->devices_param[backend_device_idx].is_opencl == true)
  {
    if ((backend_ctx->devices_param[backend_device_idx].opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) return -1;

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_AMD)
    {
    }

    if (backend_ctx->devices_param[backend_device_idx].opencl_device_vendor_id == VENDOR_ID_NV)
    {
      if (hwmon_ctx->hm_nvml)
      {
        /* this is triggered by mask generator, too. therefore useless
        unsigned long long clocksThrottleReasons = 0;
        unsigned long long supportedThrottleReasons = 0;

        if (hm_NVML_nvmlDeviceGetCurrentClocksThrottleReasons   (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, &clocksThrottleReasons)    == -1) return -1;
        if (hm_NVML_nvmlDeviceGetSupportedClocksThrottleReasons (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvml, &supportedThrottleReasons) == -1) return -1;

        clocksThrottleReasons &=  supportedThrottleReasons;
        clocksThrottleReasons &= ~nvmlClocksThrottleReasonGpuIdle;
        clocksThrottleReasons &= ~nvmlClocksThrottleReasonApplicationsClocksSetting;
        clocksThrottleReasons &= ~nvmlClocksThrottleReasonUnknown;

        if (backend_ctx->kernel_power_final)
        {
          clocksThrottleReasons &= ~nvmlClocksThrottleReasonHwSlowdown;
        }

        return (clocksThrottleReasons != nvmlClocksThrottleReasonNone);
        */
      }

      if (hwmon_ctx->hm_nvapi)
      {
        NV_GPU_PERF_POLICIES_INFO_PARAMS_V1   perfPolicies_info;
        NV_GPU_PERF_POLICIES_STATUS_PARAMS_V1 perfPolicies_status;

        memset (&perfPolicies_info,   0, sizeof (NV_GPU_PERF_POLICIES_INFO_PARAMS_V1));
        memset (&perfPolicies_status, 0, sizeof (NV_GPU_PERF_POLICIES_STATUS_PARAMS_V1));

        perfPolicies_info.version   = MAKE_NVAPI_VERSION (NV_GPU_PERF_POLICIES_INFO_PARAMS_V1, 1);
        perfPolicies_status.version = MAKE_NVAPI_VERSION (NV_GPU_PERF_POLICIES_STATUS_PARAMS_V1, 1);

        hm_NvAPI_GPU_GetPerfPoliciesInfo (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvapi, &perfPolicies_info);

        perfPolicies_status.info_value = perfPolicies_info.info_value;

        hm_NvAPI_GPU_GetPerfPoliciesStatus (hashcat_ctx, hwmon_ctx->hm_device[backend_device_idx].nvapi, &perfPolicies_status);

        return perfPolicies_status.throttle & 2;
      }
    }
  }

  hwmon_ctx->hm_device[backend_device_idx].throttle_get_supported = false;

  return -1;
}

int hwmon_ctx_init (hashcat_ctx_t *hashcat_ctx)
{
  hwmon_ctx_t    *hwmon_ctx    = hashcat_ctx->hwmon_ctx;
  backend_ctx_t  *backend_ctx  = hashcat_ctx->backend_ctx;
  user_options_t *user_options = hashcat_ctx->user_options;

  hwmon_ctx->enabled = false;

  #if !defined (WITH_HWMON)
  return 0;
  #endif // WITH_HWMON

  if (user_options->hash_info       == true) return 0;
  if (user_options->keyspace        == true) return 0;
  if (user_options->left            == true) return 0;
  if (user_options->backend_info    == true) return 0;
  if (user_options->show            == true) return 0;
  if (user_options->stdout_flag     == true) return 0;
  if (user_options->usage           == true) return 0;
  if (user_options->version         == true) return 0;
  if (user_options->hwmon_disable   == true) return 0;

  hwmon_ctx->hm_device = (hm_attrs_t *) hccalloc (DEVICES_MAX, sizeof (hm_attrs_t));

  /**
   * Initialize shared libraries
   */

  hm_attrs_t *hm_adapters_adl   = (hm_attrs_t *) hccalloc (DEVICES_MAX, sizeof (hm_attrs_t));
  hm_attrs_t *hm_adapters_nvapi = (hm_attrs_t *) hccalloc (DEVICES_MAX, sizeof (hm_attrs_t));
  hm_attrs_t *hm_adapters_nvml  = (hm_attrs_t *) hccalloc (DEVICES_MAX, sizeof (hm_attrs_t));
  hm_attrs_t *hm_adapters_sysfs = (hm_attrs_t *) hccalloc (DEVICES_MAX, sizeof (hm_attrs_t));

  #define FREE_ADAPTERS         \
  do {                          \
    hcfree (hm_adapters_adl);   \
    hcfree (hm_adapters_nvapi); \
    hcfree (hm_adapters_nvml);  \
    hcfree (hm_adapters_sysfs); \
  } while (0)

  if (backend_ctx->need_nvml == true)
  {
    hwmon_ctx->hm_nvml = (NVML_PTR *) hcmalloc (sizeof (NVML_PTR));

    if (nvml_init (hashcat_ctx) == -1)
    {
      hcfree (hwmon_ctx->hm_nvml);

      hwmon_ctx->hm_nvml = NULL;
    }
  }

  if ((backend_ctx->need_nvapi == true) && (hwmon_ctx->hm_nvml)) // nvapi can't work alone, we need nvml, too
  {
    hwmon_ctx->hm_nvapi = (NVAPI_PTR *) hcmalloc (sizeof (NVAPI_PTR));

    if (nvapi_init (hashcat_ctx) == -1)
    {
      hcfree (hwmon_ctx->hm_nvapi);

      hwmon_ctx->hm_nvapi = NULL;
    }
  }

  if (backend_ctx->need_adl == true)
  {
    hwmon_ctx->hm_adl = (ADL_PTR *) hcmalloc (sizeof (ADL_PTR));

    if (adl_init (hashcat_ctx) == -1)
    {
      hcfree (hwmon_ctx->hm_adl);

      hwmon_ctx->hm_adl = NULL;
    }
  }

  if (backend_ctx->need_sysfs == true)
  {
    hwmon_ctx->hm_sysfs = (SYSFS_PTR *) hcmalloc (sizeof (SYSFS_PTR));

    if (sysfs_init (hashcat_ctx) == false)
    {
      hcfree (hwmon_ctx->hm_sysfs);

      hwmon_ctx->hm_sysfs = NULL;
    }

    // also if there's ADL, we don't need sysfs

    if (hwmon_ctx->hm_adl)
    {
      hcfree (hwmon_ctx->hm_sysfs);

      hwmon_ctx->hm_sysfs = NULL;
    }
  }

  if (hwmon_ctx->hm_nvml)
  {
    if (hm_NVML_nvmlInit (hashcat_ctx) == 0)
    {
      HM_ADAPTER_NVML *nvmlGPUHandle = (HM_ADAPTER_NVML *) hccalloc (DEVICES_MAX, sizeof (HM_ADAPTER_NVML));

      int tmp_in = hm_get_adapter_index_nvml (hashcat_ctx, nvmlGPUHandle);

      for (int backend_devices_idx = 0; backend_devices_idx < backend_ctx->backend_devices_cnt; backend_devices_idx++)
      {
        hc_device_param_t *device_param = &backend_ctx->devices_param[backend_devices_idx];

        if (device_param->skipped == true) continue;

        if (device_param->is_cuda == true)
        {
          for (int i = 0; i < tmp_in; i++)
          {
            nvmlPciInfo_t pci;

            if (hm_NVML_nvmlDeviceGetPciInfo (hashcat_ctx, nvmlGPUHandle[i], &pci) == -1) continue;

            if ((device_param->pcie_bus      == pci.bus)
             && (device_param->pcie_device   == (pci.device >> 3))
             && (device_param->pcie_function == (pci.device & 7)))
            {
              const u32 device_id = device_param->device_id;

              hm_adapters_nvml[device_id].nvml = nvmlGPUHandle[i];

              hm_adapters_nvml[device_id].buslanes_get_supported            = true;
              hm_adapters_nvml[device_id].corespeed_get_supported           = true;
              hm_adapters_nvml[device_id].fanspeed_get_supported            = true;
              hm_adapters_nvml[device_id].memoryspeed_get_supported         = true;
              hm_adapters_nvml[device_id].temperature_get_supported         = true;
              hm_adapters_nvml[device_id].threshold_shutdown_get_supported  = true;
              hm_adapters_nvml[device_id].threshold_slowdown_get_supported  = true;
              hm_adapters_nvml[device_id].utilization_get_supported         = true;
            }
          }
        }

        if (device_param->is_opencl == true)
        {
          if ((device_param->opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) continue;

          if (device_param->opencl_device_vendor_id != VENDOR_ID_NV) continue;

          for (int i = 0; i < tmp_in; i++)
          {
            nvmlPciInfo_t pci;

            if (hm_NVML_nvmlDeviceGetPciInfo (hashcat_ctx, nvmlGPUHandle[i], &pci) == -1) continue;

            if ((device_param->pcie_bus      == pci.bus)
             && (device_param->pcie_device   == (pci.device >> 3))
             && (device_param->pcie_function == (pci.device & 7)))
            {
              const u32 device_id = device_param->device_id;

              hm_adapters_nvml[device_id].nvml = nvmlGPUHandle[i];

              hm_adapters_nvml[device_id].buslanes_get_supported            = true;
              hm_adapters_nvml[device_id].corespeed_get_supported           = true;
              hm_adapters_nvml[device_id].fanspeed_get_supported            = true;
              hm_adapters_nvml[device_id].memoryspeed_get_supported         = true;
              hm_adapters_nvml[device_id].temperature_get_supported         = true;
              hm_adapters_nvml[device_id].threshold_shutdown_get_supported  = true;
              hm_adapters_nvml[device_id].threshold_slowdown_get_supported  = true;
              hm_adapters_nvml[device_id].utilization_get_supported         = true;
            }
          }
        }
      }

      hcfree (nvmlGPUHandle);
    }
  }

  if (hwmon_ctx->hm_nvapi)
  {
    if (hm_NvAPI_Initialize (hashcat_ctx) == 0)
    {
      HM_ADAPTER_NVAPI *nvGPUHandle = (HM_ADAPTER_NVAPI *) hccalloc (NVAPI_MAX_PHYSICAL_GPUS, sizeof (HM_ADAPTER_NVAPI));

      int tmp_in = hm_get_adapter_index_nvapi (hashcat_ctx, nvGPUHandle);

      for (int backend_devices_idx = 0; backend_devices_idx < backend_ctx->backend_devices_cnt; backend_devices_idx++)
      {
        hc_device_param_t *device_param = &backend_ctx->devices_param[backend_devices_idx];

        if (device_param->skipped == true) continue;

        if (device_param->is_cuda == true)
        {
          for (int i = 0; i < tmp_in; i++)
          {
            NvU32 BusId     = 0;
            NvU32 BusSlotId = 0;

            if (hm_NvAPI_GPU_GetBusId (hashcat_ctx, nvGPUHandle[i], &BusId) == -1) continue;

            if (hm_NvAPI_GPU_GetBusSlotId (hashcat_ctx, nvGPUHandle[i], &BusSlotId) == -1) continue;

            if ((device_param->pcie_bus      == BusId)
             && (device_param->pcie_device   == (BusSlotId >> 3))
             && (device_param->pcie_function == (BusSlotId & 7)))
            {
              const u32 device_id = device_param->device_id;

              hm_adapters_nvapi[device_id].nvapi = nvGPUHandle[i];

              hm_adapters_nvapi[device_id].fanpolicy_get_supported  = true;
              hm_adapters_nvapi[device_id].throttle_get_supported   = true;
            }
          }
        }

        if (device_param->is_opencl == true)
        {
          if ((device_param->opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) continue;

          if (device_param->opencl_device_vendor_id != VENDOR_ID_NV) continue;

          for (int i = 0; i < tmp_in; i++)
          {
            NvU32 BusId     = 0;
            NvU32 BusSlotId = 0;

            if (hm_NvAPI_GPU_GetBusId (hashcat_ctx, nvGPUHandle[i], &BusId) == -1) continue;

            if (hm_NvAPI_GPU_GetBusSlotId (hashcat_ctx, nvGPUHandle[i], &BusSlotId) == -1) continue;

            if ((device_param->pcie_bus      == BusId)
             && (device_param->pcie_device   == (BusSlotId >> 3))
             && (device_param->pcie_function == (BusSlotId & 7)))
            {
              const u32 device_id = device_param->device_id;

              hm_adapters_nvapi[device_id].nvapi = nvGPUHandle[i];

              hm_adapters_nvapi[device_id].fanpolicy_get_supported  = true;
              hm_adapters_nvapi[device_id].throttle_get_supported   = true;
            }
          }
        }
      }

      hcfree (nvGPUHandle);
    }
  }

  if (hwmon_ctx->hm_adl)
  {
    if (hm_ADL_Main_Control_Create (hashcat_ctx, ADL_Main_Memory_Alloc, 0) == 0)
    {
      // total number of adapters

      int tmp_in;

      if (get_adapters_num_adl (hashcat_ctx, &tmp_in) == -1)
      {
        FREE_ADAPTERS;

        return -1;
      }

      // adapter info

      LPAdapterInfo lpAdapterInfo = (LPAdapterInfo) hccalloc (tmp_in, sizeof (AdapterInfo));

      if (hm_ADL_Adapter_AdapterInfo_Get (hashcat_ctx, lpAdapterInfo, tmp_in * sizeof (AdapterInfo)) == -1)
      {
        FREE_ADAPTERS;

        return -1;
      }

      for (int backend_devices_idx = 0; backend_devices_idx < backend_ctx->backend_devices_cnt; backend_devices_idx++)
      {
        hc_device_param_t *device_param = &backend_ctx->devices_param[backend_devices_idx];

        if (device_param->skipped == true) continue;

        if (device_param->is_cuda == true)
        {
          // nothing to do
        }

        if (device_param->is_opencl == true)
        {
          if ((device_param->opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) continue;

          if (device_param->opencl_device_vendor_id != VENDOR_ID_AMD) continue;

          for (int i = 0; i < tmp_in; i++)
          {
            if ((device_param->pcie_bus      == lpAdapterInfo[i].iBusNumber)
             && (device_param->pcie_device   == (lpAdapterInfo[i].iDeviceNumber >> 3))
             && (device_param->pcie_function == (lpAdapterInfo[i].iDeviceNumber & 7)))
            {
              const u32 device_id = device_param->device_id;

              int od_supported = 0;
              int od_enabled   = 0;
              int od_version   = 0;

              hm_ADL_Overdrive_Caps (hashcat_ctx, lpAdapterInfo[i].iAdapterIndex, &od_supported, &od_enabled, &od_version);

              hm_adapters_adl[device_id].od_version = od_version;

              hm_adapters_adl[device_id].adl = lpAdapterInfo[i].iAdapterIndex;

              hm_adapters_adl[device_id].buslanes_get_supported            = true;
              hm_adapters_adl[device_id].corespeed_get_supported           = true;
              hm_adapters_adl[device_id].fanspeed_get_supported            = true;
              hm_adapters_adl[device_id].fanpolicy_get_supported           = true;
              hm_adapters_adl[device_id].memoryspeed_get_supported         = true;
              hm_adapters_adl[device_id].temperature_get_supported         = true;
              hm_adapters_adl[device_id].threshold_slowdown_get_supported  = true;
              hm_adapters_adl[device_id].utilization_get_supported         = true;
            }
          }
        }
      }

      hcfree (lpAdapterInfo);
    }
  }

  if (hwmon_ctx->hm_sysfs)
  {
    if (true)
    {
      for (int backend_devices_idx = 0; backend_devices_idx < backend_ctx->backend_devices_cnt; backend_devices_idx++)
      {
        hc_device_param_t *device_param = &backend_ctx->devices_param[backend_devices_idx];

        if (device_param->is_cuda == true)
        {
          // nothing to do
        }

        if (device_param->is_opencl == true)
        {
          const u32 device_id = device_param->device_id;

          if ((device_param->opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) continue;

          hm_adapters_sysfs[device_id].buslanes_get_supported    = true;
          hm_adapters_sysfs[device_id].corespeed_get_supported   = true;
          hm_adapters_sysfs[device_id].fanspeed_get_supported    = true;
          hm_adapters_sysfs[device_id].fanpolicy_get_supported   = true;
          hm_adapters_sysfs[device_id].memoryspeed_get_supported = true;
          hm_adapters_sysfs[device_id].temperature_get_supported = true;
          hm_adapters_sysfs[device_id].utilization_get_supported = true;
        }
      }
    }
  }

  if (hwmon_ctx->hm_adl == NULL && hwmon_ctx->hm_nvml == NULL && hwmon_ctx->hm_sysfs == NULL)
  {
    FREE_ADAPTERS;

    return 0;
  }

  /**
   * looks like we have some manageable device
   */

  hwmon_ctx->enabled = true;

  /**
   * save buffer required for later restores
   */

  hwmon_ctx->od_clock_mem_status = (ADLOD6MemClockState *) hccalloc (backend_ctx->backend_devices_cnt, sizeof (ADLOD6MemClockState));

  /**
   * HM devices: copy
   */

  for (int backend_devices_idx = 0; backend_devices_idx < backend_ctx->backend_devices_cnt; backend_devices_idx++)
  {
    hc_device_param_t *device_param = &backend_ctx->devices_param[backend_devices_idx];

    if (device_param->skipped == true) continue;

    const u32 device_id = device_param->device_id;

    if (device_param->is_cuda == true)
    {
      hwmon_ctx->hm_device[backend_devices_idx].adl         = 0;
      hwmon_ctx->hm_device[backend_devices_idx].sysfs       = 0;
      hwmon_ctx->hm_device[backend_devices_idx].nvapi       = hm_adapters_nvapi[device_id].nvapi;
      hwmon_ctx->hm_device[backend_devices_idx].nvml        = hm_adapters_nvml[device_id].nvml;
      hwmon_ctx->hm_device[backend_devices_idx].od_version  = 0;

      if (hwmon_ctx->hm_nvml)
      {
        hwmon_ctx->hm_device[backend_devices_idx].buslanes_get_supported            |= hm_adapters_nvml[device_id].buslanes_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].corespeed_get_supported           |= hm_adapters_nvml[device_id].corespeed_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].fanspeed_get_supported            |= hm_adapters_nvml[device_id].fanspeed_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].fanpolicy_get_supported           |= hm_adapters_nvml[device_id].fanpolicy_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].memoryspeed_get_supported         |= hm_adapters_nvml[device_id].memoryspeed_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].temperature_get_supported         |= hm_adapters_nvml[device_id].temperature_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].threshold_shutdown_get_supported  |= hm_adapters_nvml[device_id].threshold_shutdown_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].threshold_slowdown_get_supported  |= hm_adapters_nvml[device_id].threshold_slowdown_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].throttle_get_supported            |= hm_adapters_nvml[device_id].throttle_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].utilization_get_supported         |= hm_adapters_nvml[device_id].utilization_get_supported;
      }

      if (hwmon_ctx->hm_nvapi)
      {
        hwmon_ctx->hm_device[backend_devices_idx].buslanes_get_supported            |= hm_adapters_nvapi[device_id].buslanes_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].corespeed_get_supported           |= hm_adapters_nvapi[device_id].corespeed_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].fanspeed_get_supported            |= hm_adapters_nvapi[device_id].fanspeed_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].fanpolicy_get_supported           |= hm_adapters_nvapi[device_id].fanpolicy_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].memoryspeed_get_supported         |= hm_adapters_nvapi[device_id].memoryspeed_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].temperature_get_supported         |= hm_adapters_nvapi[device_id].temperature_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].threshold_shutdown_get_supported  |= hm_adapters_nvapi[device_id].threshold_shutdown_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].threshold_slowdown_get_supported  |= hm_adapters_nvapi[device_id].threshold_slowdown_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].throttle_get_supported            |= hm_adapters_nvapi[device_id].throttle_get_supported;
        hwmon_ctx->hm_device[backend_devices_idx].utilization_get_supported         |= hm_adapters_nvapi[device_id].utilization_get_supported;
      }
    }

    if (device_param->is_opencl == true)
    {
      if ((device_param->opencl_device_type & CL_DEVICE_TYPE_GPU) == 0) continue;

      if (device_param->opencl_device_vendor_id == VENDOR_ID_AMD)
      {
        hwmon_ctx->hm_device[backend_devices_idx].adl         = hm_adapters_adl[device_id].adl;
        hwmon_ctx->hm_device[backend_devices_idx].sysfs       = hm_adapters_sysfs[device_id].sysfs; // not used
        hwmon_ctx->hm_device[backend_devices_idx].nvapi       = 0;
        hwmon_ctx->hm_device[backend_devices_idx].nvml        = 0;
        hwmon_ctx->hm_device[backend_devices_idx].od_version  = 0;

        if (hwmon_ctx->hm_adl)
        {
          hwmon_ctx->hm_device[backend_devices_idx].od_version = hm_adapters_adl[device_id].od_version;

          hwmon_ctx->hm_device[backend_devices_idx].buslanes_get_supported            |= hm_adapters_adl[device_id].buslanes_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].corespeed_get_supported           |= hm_adapters_adl[device_id].corespeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].fanspeed_get_supported            |= hm_adapters_adl[device_id].fanspeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].fanpolicy_get_supported           |= hm_adapters_adl[device_id].fanpolicy_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].memoryspeed_get_supported         |= hm_adapters_adl[device_id].memoryspeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].temperature_get_supported         |= hm_adapters_adl[device_id].temperature_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].threshold_shutdown_get_supported  |= hm_adapters_adl[device_id].threshold_shutdown_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].threshold_slowdown_get_supported  |= hm_adapters_adl[device_id].threshold_slowdown_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].throttle_get_supported            |= hm_adapters_adl[device_id].throttle_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].utilization_get_supported         |= hm_adapters_adl[device_id].utilization_get_supported;
        }

        if (hwmon_ctx->hm_sysfs)
        {
          hwmon_ctx->hm_device[backend_devices_idx].buslanes_get_supported            |= hm_adapters_sysfs[device_id].buslanes_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].corespeed_get_supported           |= hm_adapters_sysfs[device_id].corespeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].fanspeed_get_supported            |= hm_adapters_sysfs[device_id].fanspeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].fanpolicy_get_supported           |= hm_adapters_sysfs[device_id].fanpolicy_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].memoryspeed_get_supported         |= hm_adapters_sysfs[device_id].memoryspeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].temperature_get_supported         |= hm_adapters_sysfs[device_id].temperature_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].threshold_shutdown_get_supported  |= hm_adapters_sysfs[device_id].threshold_shutdown_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].threshold_slowdown_get_supported  |= hm_adapters_sysfs[device_id].threshold_slowdown_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].throttle_get_supported            |= hm_adapters_sysfs[device_id].throttle_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].utilization_get_supported         |= hm_adapters_sysfs[device_id].utilization_get_supported;
        }
      }

      if (device_param->opencl_device_vendor_id == VENDOR_ID_NV)
      {
        hwmon_ctx->hm_device[backend_devices_idx].adl         = 0;
        hwmon_ctx->hm_device[backend_devices_idx].sysfs       = 0;
        hwmon_ctx->hm_device[backend_devices_idx].nvapi       = hm_adapters_nvapi[device_id].nvapi;
        hwmon_ctx->hm_device[backend_devices_idx].nvml        = hm_adapters_nvml[device_id].nvml;
        hwmon_ctx->hm_device[backend_devices_idx].od_version  = 0;

        if (hwmon_ctx->hm_nvml)
        {
          hwmon_ctx->hm_device[backend_devices_idx].buslanes_get_supported            |= hm_adapters_nvml[device_id].buslanes_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].corespeed_get_supported           |= hm_adapters_nvml[device_id].corespeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].fanspeed_get_supported            |= hm_adapters_nvml[device_id].fanspeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].fanpolicy_get_supported           |= hm_adapters_nvml[device_id].fanpolicy_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].memoryspeed_get_supported         |= hm_adapters_nvml[device_id].memoryspeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].temperature_get_supported         |= hm_adapters_nvml[device_id].temperature_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].threshold_shutdown_get_supported  |= hm_adapters_nvml[device_id].threshold_shutdown_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].threshold_slowdown_get_supported  |= hm_adapters_nvml[device_id].threshold_slowdown_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].throttle_get_supported            |= hm_adapters_nvml[device_id].throttle_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].utilization_get_supported         |= hm_adapters_nvml[device_id].utilization_get_supported;
        }

        if (hwmon_ctx->hm_nvapi)
        {
          hwmon_ctx->hm_device[backend_devices_idx].buslanes_get_supported            |= hm_adapters_nvapi[device_id].buslanes_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].corespeed_get_supported           |= hm_adapters_nvapi[device_id].corespeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].fanspeed_get_supported            |= hm_adapters_nvapi[device_id].fanspeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].fanpolicy_get_supported           |= hm_adapters_nvapi[device_id].fanpolicy_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].memoryspeed_get_supported         |= hm_adapters_nvapi[device_id].memoryspeed_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].temperature_get_supported         |= hm_adapters_nvapi[device_id].temperature_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].threshold_shutdown_get_supported  |= hm_adapters_nvapi[device_id].threshold_shutdown_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].threshold_slowdown_get_supported  |= hm_adapters_nvapi[device_id].threshold_slowdown_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].throttle_get_supported            |= hm_adapters_nvapi[device_id].throttle_get_supported;
          hwmon_ctx->hm_device[backend_devices_idx].utilization_get_supported         |= hm_adapters_nvapi[device_id].utilization_get_supported;
        }
      }
    }

    // by calling the different functions here this will disable them in case they will error out
    // this will also reduce the error itself printed to the user to a single print on startup

    hm_get_buslanes_with_devices_idx           (hashcat_ctx, backend_devices_idx);
    hm_get_corespeed_with_devices_idx          (hashcat_ctx, backend_devices_idx);
    hm_get_fanpolicy_with_devices_idx          (hashcat_ctx, backend_devices_idx);
    hm_get_fanspeed_with_devices_idx           (hashcat_ctx, backend_devices_idx);
    hm_get_memoryspeed_with_devices_idx        (hashcat_ctx, backend_devices_idx);
    hm_get_temperature_with_devices_idx        (hashcat_ctx, backend_devices_idx);
    hm_get_threshold_shutdown_with_devices_idx (hashcat_ctx, backend_devices_idx);
    hm_get_threshold_slowdown_with_devices_idx (hashcat_ctx, backend_devices_idx);
    hm_get_throttle_with_devices_idx           (hashcat_ctx, backend_devices_idx);
    hm_get_utilization_with_devices_idx        (hashcat_ctx, backend_devices_idx);
  }

  FREE_ADAPTERS;

  return 0;
}

void hwmon_ctx_destroy (hashcat_ctx_t *hashcat_ctx)
{
  hwmon_ctx_t *hwmon_ctx = hashcat_ctx->hwmon_ctx;

  if (hwmon_ctx->enabled == false) return;

  // unload shared libraries

  if (hwmon_ctx->hm_nvml)
  {
    hm_NVML_nvmlShutdown (hashcat_ctx);

    nvml_close (hashcat_ctx);
  }

  if (hwmon_ctx->hm_nvapi)
  {
    hm_NvAPI_Unload (hashcat_ctx);

    nvapi_close (hashcat_ctx);
  }

  if (hwmon_ctx->hm_adl)
  {
    hm_ADL_Main_Control_Destroy (hashcat_ctx);

    adl_close (hashcat_ctx);
  }

  if (hwmon_ctx->hm_sysfs)
  {
    sysfs_close (hashcat_ctx);
  }

  // free memory

  hcfree (hwmon_ctx->od_clock_mem_status);

  hcfree (hwmon_ctx->hm_device);

  memset (hwmon_ctx, 0, sizeof (hwmon_ctx_t));
}
