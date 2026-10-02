"""Wire responses; deliberately independent of Qt."""
from dataclasses import dataclass


@dataclass(frozen=True)
class Response:
    kind: str
    fields: dict[str, str]
    text: str


@dataclass(frozen=True)
class Reply:
    command: str
    responses: tuple[Response, ...]
    error: str | None = None
