/* Copyright 2013-present Facebook, Inc.
 * Licensed under the Apache License, Version 2.0 */

#include "watchman.h"

/* query /root {query} */
static void cmd_query(struct watchman_client* client, const json_ref& args) {
  char *errmsg = NULL;
  w_query_res res;
  char clockbuf[128];
  struct unlocked_watchman_root unlocked;

  if (json_array_size(args) != 3) {
    send_error_response(client, "wrong number of arguments for 'query'");
    return;
  }

  if (!resolve_root_or_err(client, args, 1, false, &unlocked)) {
    return;
  }

  const auto& query_spec = args.at(2);
  auto query = w_query_parse(unlocked.root, query_spec, &errmsg);
  if (!query) {
    send_error_response(client, "failed to parse query: %s", errmsg);
    free(errmsg);
    w_root_delref(&unlocked);
    return;
  }

  if (client->client_mode) {
    query->sync_timeout = std::chrono::milliseconds(0);
  }

  if (!w_query_execute(query.get(), &unlocked, &res, nullptr)) {
    send_error_response(client, "query failed: %s", res.errmsg);
    w_root_delref(&unlocked);
    return;
  }

  auto response = make_response();
  if (clock_id_string(res.root_number, res.ticks, clockbuf, sizeof(clockbuf))) {
    response.set("clock", typed_string_to_json(clockbuf, W_STRING_UNICODE));
  }
  response.set({{"is_fresh_instance", json_boolean(res.is_fresh_instance)},
                {"files", std::move(res.resultsArray)}});

  {
    struct read_locked_watchman_root lock;
    w_root_read_lock(&unlocked, "obtain_warnings", &lock);
    add_root_warnings_to_response(response, &lock);
    w_root_read_unlock(&lock, &unlocked);
  }

  send_and_dispose_response(client, std::move(response));
  w_root_delref(&unlocked);
}
W_CMD_REG("query", cmd_query, CMD_DAEMON | CMD_CLIENT | CMD_ALLOW_ANY_USER,
          w_cmd_realpath_root)

/* vim:ts=2:sw=2:et:
 */
