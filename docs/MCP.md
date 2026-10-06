# Local MCP server

The optional stdio server exposes only these read-only tools:

`get_capabilities`, `get_system_info`, `sample_system_metrics`,
`list_processes`, `inspect_process`, `list_startup_entries`, `scan_files`,
`inspect_file`, `get_directory_usage`, `find_hardlinks`,
`diagnose_system_pressure`, and `investigate_file_indicators`.

No MCP tool offers shell execution, tracing, network access, writes, package
installation, quarantine, process signals, or session termination. In
particular, `traceflow` is deliberately outside MCP and is not a sandbox.

Example client configuration (replace both absolute paths):

```json
{
  "mcpServers": {
    "pydev": {
      "command": "/absolute/path/to/venv/bin/python",
      "args": [
        "-m", "pydev_ai.mcp_server",
        "--root", "/absolute/approved/root"
      ]
    }
  }
}
```

Repeat `--root` to authorize another tree. Configure the smallest practical
roots. The server resolves paths before access, refuses escapes, limits a
single response to 4 MiB, allows at most four collectors concurrently, and
requires bounded tool arguments. It needs no API key and receives no network
permission.

Run `make mcp-check` after installing `.[mcp]`. That test starts the server over
stdio with an actual SDK client, inventories its tools, and calls two tools.

Implementation references: the official
[Model Context Protocol Python SDK](https://github.com/modelcontextprotocol/python-sdk),
its [client transport guide](https://github.com/modelcontextprotocol/python-sdk/blob/main/docs/client/transports.md),
and [structured tool documentation](https://github.com/modelcontextprotocol/python-sdk/blob/main/docs/servers/tools.md).
