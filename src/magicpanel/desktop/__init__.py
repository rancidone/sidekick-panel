"""Desktop-side event emitters (the Mac side of the system).

Everything under here *produces* semantic events and hands them to the
engine over the local socket. It is the counterpart to the scenes/engine,
which *consume* events. Adapters here watch real desktop activity (git
commits, CI runs) and translate it into the sanitized, high-level events
the engine understands — never sending code, logs, branch names, or commit
messages, per the project constraints (docs/discovery-brief.md).
"""
