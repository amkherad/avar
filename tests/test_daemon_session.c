#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include <daemon/daemon_session.h>

static TestGuard g_guard;

static void setup_local_session(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-session"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_LOCAL), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_TRANSPORT, AVAR_DAEMON_TRANSPORT_LOCAL), 0);
}

AVAR_TEST(daemon_session_init_local_mode) {
    setup_local_session();
    daemon_session_deinit();

    AVAR_ASSERT_EQ(daemon_session_init(), DaemonSessionErrorNone);
    AVAR_ASSERT_EQ(daemon_session_mode(), AvarSessionModeLocal);
    AVAR_ASSERT(!daemon_session_is_remote());
    AVAR_ASSERT_EQ(daemon_session_transport(), AvarTransportLocal);
    AVAR_ASSERT_NOT_NULL(daemon_session_config());

    daemon_session_deinit();
    AVAR_ASSERT_NULL(daemon_session_config());
}

AVAR_TEST(daemon_session_force_local_overrides_remote) {
    setup_local_session();
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_REMOTE), 0);
    daemon_session_deinit();

    AVAR_ASSERT_EQ(daemon_session_init(), DaemonSessionErrorNone);
    daemon_session_force_local(true);
    AVAR_ASSERT(!daemon_session_is_remote());

    daemon_session_deinit();
}

AVAR_TEST(daemon_session_delegate_local_is_noop) {
    setup_local_session();
    daemon_session_deinit();
    AVAR_ASSERT_EQ(daemon_session_init(), DaemonSessionErrorNone);

    AVAR_ASSERT_EQ(daemon_session_delegate("download.start", "{}"), DaemonSessionErrorNone);

    daemon_session_deinit();
}

AVAR_TEST_MAIN(
        run_daemon_session_init_local_mode();
        run_daemon_session_force_local_overrides_remote();
        run_daemon_session_delegate_local_is_noop();)
