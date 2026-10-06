"""Exercise initialize/discovery, tools/list, and a real stdio tool call."""

from __future__ import annotations

import asyncio
import sys
from pathlib import Path

from mcp import Client, StdioServerParameters


async def integration() -> None:
    root = Path(__file__).resolve().parents[1]
    parameters = StdioServerParameters(
        command=sys.executable,
        args=["-m", "pydev_ai.mcp_server", "--root", str(root)],
        cwd=str(root),
    )
    async with Client(parameters, read_timeout_seconds=20) as client:
        if not client.protocol_version:
            raise AssertionError("MCP protocol negotiation did not complete")
        tools = await client.list_tools()
        names = {tool.name for tool in tools.tools}
        required = {"get_capabilities", "get_system_info", "inspect_file", "scan_files", "find_hardlinks"}
        if not required.issubset(names):
            raise AssertionError("missing MCP tools: {}".format(sorted(required - names)))
        result = await client.call_tool("get_capabilities", {})
        if result.is_error or not result.structured_content:
            raise AssertionError("get_capabilities failed")
        response = result.structured_content
        if response.get("schema_version") != "1.0" or response.get("status") != "success":
            raise AssertionError("invalid structured response")
        file_result = await client.call_tool("inspect_file", {"path": str(root / "README.md"), "hex_bytes": 32})
        if file_result.is_error or file_result.structured_content.get("data", {}).get("sha256") is None:
            raise AssertionError("inspect_file did not return file evidence")


if __name__ == "__main__":
    asyncio.run(integration())
    print("MCP stdio integration passed")
