#ifndef AVAR_GUI_LAUNCH_H
#define AVAR_GUI_LAUNCH_H

/**
 * All-in-one GUI entry hook. Returns a process exit code when argc indicates
 * GUI mode (no subcommand or "gui"), otherwise -1 so normal CLI dispatch runs.
 */
int cli_embed_gui_early(int argc, char *argv[]);

#endif
