# The loudness a process plays at, from Windows' own audio sessions (Core Audio: IAudioMeterInformation per session):
# an oracle that a game's sounds really come out, not only that it asked for them.
#   powershell -File audio_meter.ps1 -ProcessId <pid> [-Seconds 6]
# Prints, per render device, the process' sessions (muted? volume) and then every 0.25 s the highest peak (0..1).
param([Parameter(Mandatory = $true)][int]$ProcessId, [double]$Seconds = 6)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace AudioMeter {
  [Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IMMDeviceEnumerator {
    int EnumAudioEndpoints(int dataFlow, int stateMask, out IMMDeviceCollection devices);
    int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice device);
  }
  [Guid("0BD7A1BE-7A1A-44DB-8397-CC5392387B5E"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IMMDeviceCollection {
    int GetCount(out int count);
    int Item(int index, out IMMDevice device);
  }
  [Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IMMDevice {
    int Activate(ref Guid iid, int clsCtx, IntPtr activationParams, [MarshalAs(UnmanagedType.IUnknown)] out object iface);
    int OpenPropertyStore(int access, out IntPtr store);
    int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id);
  }
  [Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioSessionManager2 {
    int GetAudioSessionControl(IntPtr guid, int flags, out IntPtr control);
    int GetSimpleAudioVolume(IntPtr guid, int flags, out IntPtr volume);
    int GetSessionEnumerator(out IAudioSessionEnumerator sessions);
  }
  [Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioSessionEnumerator {
    int GetCount(out int count);
    int GetSession(int index, out IAudioSessionControl2 session);
  }
  [Guid("bfb7ff88-7239-4fc9-8fa2-07c950be9c6d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioSessionControl2 {
    int GetState(out int state);
    int GetDisplayName(IntPtr name);
    int SetDisplayName(IntPtr name, IntPtr context);
    int GetIconPath(IntPtr path);
    int SetIconPath(IntPtr path, IntPtr context);
    int GetGroupingParam(out Guid param);
    int SetGroupingParam(IntPtr param, IntPtr context);
    int RegisterAudioSessionNotification(IntPtr n);
    int UnregisterAudioSessionNotification(IntPtr n);
    int GetSessionIdentifier(IntPtr id);
    int GetSessionInstanceIdentifier(IntPtr id);
    int GetProcessId(out int pid);
  }
  [Guid("C02216F6-8C67-4B5B-9D00-D008E73E0064"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioMeterInformation {
    int GetPeakValue(out float peak);
  }
  [Guid("87CE5498-68D6-44E5-9215-6DA47EF883D8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface ISimpleAudioVolume {
    int SetMasterVolume(float level, IntPtr context);
    int GetMasterVolume(out float level);
    int SetMute(int mute, IntPtr context);
    int GetMute(out int mute);
  }
  [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator {}

  public static class Meter {
    static List<IAudioMeterInformation> _meters = new List<IAudioMeterInformation>();

    public static string Find(int pid) {
      string strOut = "";
      IMMDeviceEnumerator en = (IMMDeviceEnumerator)new MMDeviceEnumerator();
      IMMDevice def; en.GetDefaultAudioEndpoint(0, 1, out def);
      string strDef; def.GetId(out strDef);
      IMMDeviceCollection all; en.EnumAudioEndpoints(0, 1, out all);
      int ct; all.GetCount(out ct);
      for (int i = 0; i < ct; i++) {
        IMMDevice dev; all.Item(i, out dev);
        string strId; dev.GetId(out strId);
        Guid iid = typeof(IAudioSessionManager2).GUID;
        object o; dev.Activate(ref iid, 23, IntPtr.Zero, out o);
        IAudioSessionManager2 mgr = (IAudioSessionManager2)o;
        IAudioSessionEnumerator sessions; mgr.GetSessionEnumerator(out sessions);
        int ctS; sessions.GetCount(out ctS);
        for (int s = 0; s < ctS; s++) {
          IAudioSessionControl2 ctl; sessions.GetSession(s, out ctl);
          int p; ctl.GetProcessId(out p);
          if (p != pid) continue;
          int state; ctl.GetState(out state);
          ISimpleAudioVolume vol = (ISimpleAudioVolume)ctl;
          float fVol; vol.GetMasterVolume(out fVol);
          int iMute; vol.GetMute(out iMute);
          _meters.Add((IAudioMeterInformation)ctl);
          strOut += String.Format("session on device {0}{1}: state {2}, muted {3}, volume {4:0.00}\n", strId,
            strId == strDef ? " (the default)" : "", state == 1 ? "active" : state == 0 ? "inactive" : "expired", iMute != 0, fVol);
        }
      }
      return strOut == "" ? "no audio session of the process on any active render device\n" : strOut;
    }

    public static float Peak() {
      float fMax = 0;
      foreach (IAudioMeterInformation m in _meters) { float f; m.GetPeakValue(out f); if (f > fMax) fMax = f; }
      return fMax;
    }
  }
}
"@

Write-Output ([AudioMeter.Meter]::Find($ProcessId)).TrimEnd()
$end = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $end) {
  $peak = 0.0
  for ($i = 0; $i -lt 5; $i++) {
    $p = [AudioMeter.Meter]::Peak()
    if ($p -gt $peak) { $peak = $p }
    Start-Sleep -Milliseconds 50
  }
  Write-Output ("peak {0:0.000}" -f $peak)
}
