# Serena MCP Server Docker Setup

This guide explains how to run the Serena MCP (Model Context Protocol) server using Docker for the edl2ffmpeg project.

## Prerequisites

- Docker and Docker Compose installed
- Access to pull from `ghcr.io` (GitHub Container Registry)

## Quick Start

1. **Start the Serena MCP server:**
   ```bash
   docker-compose up -d serena-mcp
   ```

2. **View logs:**
   ```bash
   docker-compose logs -f serena-mcp
   ```

3. **Stop the server:**
   ```bash
   docker-compose down
   ```

## Configuration

### Project Configuration

The Serena project configuration is located in `.serena/project.yml`. This file defines:
- Language settings (C++ with clangd support)
- Include/exclude patterns for file indexing
- Search and editor preferences

### Claude Desktop Integration

To use the Dockerized Serena server with Claude Desktop:

1. Copy the example configuration:
   ```bash
   cp claude_desktop_config.example.json ~/Library/Application\ Support/Claude/claude_desktop_config.json
   ```

2. Adjust the volume mount path in the configuration if your project is located elsewhere.

### Docker Compose Services

The `docker-compose.yml` defines:
- **serena-mcp**: The Serena MCP server with the project mounted at `/workspaces/edl2ffmpeg`

## Environment Variables

You can set these in the `docker-compose.yml`:
- `SERENA_LOG_LEVEL`: Controls logging verbosity (DEBUG, INFO, WARNING, ERROR)
- `INTELEPHENSE_LICENSE_KEY`: For PHP premium features (if needed)

## Volume Mounts

The current configuration mounts:
- `.:/workspaces/edl2ffmpeg`: Your project directory

You can add additional mounts for other projects by uncommenting and modifying the example in `docker-compose.yml`.

## Troubleshooting

1. **Container won't start:**
   - Check Docker daemon is running: `docker ps`
   - Verify image can be pulled: `docker pull ghcr.io/oraios/serena:latest`

2. **MCP connection issues:**
   - Ensure `--network host` is used (required for MCP)
   - Check container logs: `docker-compose logs serena-mcp`

3. **File access problems:**
   - Verify volume mount paths are correct
   - Check file permissions in mounted directories

## Security Notes

- The container runs with `--network host` for MCP compatibility
- Files are mounted read-write (`:rw`) for editing capabilities
- Consider using read-only mounts for sensitive directories

## Advanced Usage

### Running with custom Serena image:
```bash
docker run --rm -i --network host \
  -v $(pwd):/workspaces/edl2ffmpeg \
  -e SERENA_LOG_LEVEL=DEBUG \
  ghcr.io/oraios/serena:latest \
  serena start-mcp-server --transport stdio
```

### Building a custom image with additional tools:
Create a `Dockerfile`:
```dockerfile
FROM ghcr.io/oraios/serena:latest
RUN apt-get update && apt-get install -y additional-tools
```

Then update `docker-compose.yml` to build from this Dockerfile.

## References

- [Serena GitHub Repository](https://github.com/oraios/serena)
- [Serena Docker Documentation](https://github.com/oraios/serena/blob/main/DOCKER.md)
- [Model Context Protocol (MCP)](https://modelcontextprotocol.io/)