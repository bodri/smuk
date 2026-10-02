from .responses import Reply, Response


class LineDecoder:
    """Serial reads may split any character or contain several lines."""
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data: bytes) -> list[str]:
        self.buffer.extend(data)
        lines = []
        while b"\n" in self.buffer:
            raw, _, remaining = self.buffer.partition(b"\n")
            self.buffer = bytearray(remaining)
            if len(raw) > 8192:
                raise ValueError("Serial line exceeds 8 KiB")
            text = raw.rstrip(b"\r").decode("ascii", errors="replace")
            if text:
                lines.append(text)
        if len(self.buffer) > 8192:
            raise ValueError("Unterminated serial line exceeds 8 KiB")
        return lines


def parse_line(text: str) -> Response:
    kind = text.split(" ", 1)[0]
    fields = {}
    for token in text.split()[1:]:
        if "=" in token:
            key, value = token.split("=", 1)
            if key in fields and kind in {"STATUS", "MEAS", "RAW", "QUALITY", "ACQ", "WATCHDOG", "CAL", "I", "V", "BUS", "CAPTURE", "POINT", "OK", "ERR"}:
                raise ValueError(f"Duplicate response field: {key}")
            fields[key] = value
    if text.startswith("SMUK serial ready") or "ADS131M03 bring-up" in text:
        kind = "STARTUP"
    return Response(kind, fields, text)


class Transaction:
    """Known firmware multiline boundaries, never an arbitrary idle timeout."""
    def __init__(self, command: str):
        self.command = command
        self.responses: list[Response] = []
        self.primary_seen = False

    def accept(self, response: Response) -> Reply | None:
        if response.kind == "ERR":
            return Reply(self.command, tuple(self.responses), response.text)
        primary = {"PING": "PONG", "STATUS?": "STATUS", "MEAS?": "MEAS", "RAW?": "RAW", "ACQ?": "ACQ", "CAL:SHOW?": "CAL"}.get(self.command)
        if primary:
            if response.kind == primary:
                self.primary_seen = True
                self.responses.append(response)
                if self.command in ("PING", "ACQ?"):
                    return Reply(self.command, tuple(self.responses))
            elif self.primary_seen:
                if self.command == "STATUS?" and response.kind in ("QUALITY", "WATCHDOG"):
                    self.responses.append(response)
                    if response.kind == "WATCHDOG" and any(r.kind == "QUALITY" for r in self.responses):
                        return Reply(self.command, tuple(self.responses))
                elif self.command in ("MEAS?", "RAW?") and response.kind == "QUALITY":
                    self.responses.append(response)
                    return Reply(self.command, tuple(self.responses))
                elif self.command == "CAL:SHOW?" and response.kind in ("I", "V", "BUS", "CAPTURE"):
                    self.responses.append(response)
                    if response.text.startswith("CAPTURE LIMIT "):
                        return Reply(self.command, tuple(self.responses))
        elif self.command == "HELP":
            # HELP consists of four lines, with PING at the end of the last.
            if response.text.startswith("STATUS? ACQ?"):
                self.primary_seen = True
            if self.primary_seen and response.kind in ("STATUS?", "RANGE:I", "CAL:BEGIN", "CAL:SAVE;"):
                self.responses.append(response)
                if response.text.endswith("; PING"):
                    return Reply(self.command, tuple(self.responses))
        elif self.command == "CAL:POINTS?":
            if response.kind == "POINT":
                self.responses.append(response)
            elif response.kind == "OK" and "points" in response.fields:
                self.responses.append(response)
                return Reply(self.command, tuple(self.responses))
        elif response.kind == "OK":
            return Reply(self.command, (response,))
        return None
