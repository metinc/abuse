# Self-hosted online multiplayer

The installer sets up the Abuse signaling service and coturn in STUN-only
mode. Game traffic remains peer-to-peer; the server does not provide TURN
relay traffic.

## Requirements

- a Debian-based server with a public IPv4 address;
- Caddy installed and `/etc/caddy/Caddyfile` present;
- a DNS A record pointing the chosen domain to the server;
- inbound TCP ports 80 and 443 and UDP port 3478 open.

## Install

Copy this directory to the server, then run:

```sh
sudo ./systemd/install.sh abusecoop.com
```

The domain argument is optional and defaults to `abusecoop.com`. The script
installs the required packages and configures signaling, STUN, systemd, and
Caddy. Copy the current `online` directory to the server and run the same
command again to update. Updating restarts signaling and closes existing rooms.

Verify the deployment:

```sh
curl https://abusecoop.com/health
systemctl --no-pager status abuse-signaling abuse-stun
```

The health endpoint should return an object containing `"ok":true`.

## Logs

```sh
journalctl -u abuse-signaling -u abuse-stun -n 100 --no-pager
```
