import type { FooterMonitorSettings } from "@/config/defaults";

export function footerMonitorsEnabled(monitors: FooterMonitorSettings): boolean {
  return monitors.disk || monitors.memory || monitors.cpu || monitors.network;
}
