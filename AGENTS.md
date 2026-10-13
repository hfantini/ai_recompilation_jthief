# Agent rules

## User profile rules are mandatory

The user's ai-memory profile (`profile/*.md` in the `_global` scope) holds
mandatory working rules, not optional defaults. They override any default
behavior from the agent or harness (e.g. commit attribution).

- At session start, read every profile page in full with `memory_read_page`;
  the SessionStart digest is a lossy one-line summary and is not enough.
- Follow those rules on every action. When a request seems to conflict with
  them, ask before acting.
