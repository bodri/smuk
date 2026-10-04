from pathlib import Path
import pytest
from smuk_desktop.protocol.parser import LineDecoder, parse_line, Transaction
from smuk_desktop.protocol.commands import validate_command
from smuk_desktop.instrument.state import Measurement

FIXTURE = Path(__file__).parent / "fixtures/serial_sessions/healthy.txt"


def test_fragmented_input_and_multiple_lines():
    decoder = LineDecoder()
    assert decoder.feed(b"MEAS V_V=3") == []
    assert decoder.feed(b"\r\nQUALITY valid=1\r\npartial") == ["MEAS V_V=3", "QUALITY valid=1"]
    assert decoder.feed(b"\n") == ["partial"]
    with pytest.raises(ValueError):
        decoder.feed(b"x" * 8193)


def test_status_does_not_complete_before_watchdog_or_on_logging():
    tx = Transaction("STATUS?")
    lines = FIXTURE.read_text().splitlines()
    assert tx.accept(parse_line(lines[0])) is None
    assert tx.accept(parse_line(lines[2])) is None
    assert tx.accept(parse_line("ADC unrelated log")) is None
    assert tx.accept(parse_line(lines[3])) is None
    reply = tx.accept(parse_line(lines[4]))
    assert [r.kind for r in reply.responses] == ["STATUS", "QUALITY", "WATCHDOG"]


def test_measurement_needs_matching_quality():
    lines = FIXTURE.read_text().splitlines()
    tx = Transaction("MEAS?")
    assert tx.accept(parse_line(lines[3])) is None  # unrelated preceding quality
    assert tx.accept(parse_line(lines[5])) is None
    reply = tx.accept(parse_line(lines[6]))
    m = Measurement.from_responses(*reply.responses)
    assert m.valid and m.voltage == 3.008601856 and m.current == 1e-9
    assert m.populated == m.window == 32
    malformed = parse_line(lines[5].replace("3.008601856", "nan"))
    with pytest.raises(ValueError):
        Measurement.from_responses(malformed, reply.responses[1])
    warmup = parse_line(lines[5].replace("32/32", "2/32"))
    assert not Measurement.from_responses(warmup, reply.responses[1]).valid


def test_errors_echoes_and_calibration_boundary():
    tx = Transaction("RANGE:V 6V")
    assert tx.accept(parse_line("RANGE:V 6V")) is None
    assert tx.accept(parse_line("ERR request busy")).error == "ERR request busy"
    tx = Transaction("CAL:SHOW?")
    assert tx.accept(parse_line("CAL active sequence=2 source=FLASH")) is None
    assert tx.accept(parse_line("BUS gain=1 offset_V=0")) is None
    assert tx.accept(parse_line("CAPTURE LIMIT rms_codes=4096 drift_codes=8192 minimum_fit_span_codes=64"))


def test_command_injection_and_unknown_commands_rejected():
    assert validate_command(" autorange:v on ") == "AUTORANGE:V ON"
    for command in ("PING\nCAL:SAVE", "OUT ON", "RANGE:V 20V", "CAL:SAVE", "", "ECHO MAYBE"):
        with pytest.raises(ValueError):
            validate_command(command)
    with pytest.raises(ValueError):
        parse_line("MEAS valid=1 valid=0")
    assert parse_line("ADC diagnostics x=1 x=2").kind == "ADC"


def test_32ksps_precision_metadata():
    from test_controller import MEAS, QUALITY
    text = MEAS.replace("window=32/32", "window=80/80") + " rate_hz=32000 group_samples=8 integration_ms=20"
    m = Measurement.from_responses(parse_line(text), parse_line(QUALITY))
    assert m.valid and m.sample_rate_hz == 32000 and m.group_samples == 8 and m.integration_ms == 20
    assert m.window * m.group_samples == 640
    with pytest.raises(ValueError):
        Measurement.from_responses(parse_line(text.replace("group_samples=8", "group_samples=1")), parse_line(QUALITY))


def test_submillisecond_integration():
    from test_controller import MEAS, QUALITY
    text = MEAS.replace("window=32/32", "window=2/2") + " rate_hz=32000 group_samples=8 integration_ms=0 integration_us=500"
    m = Measurement.from_responses(parse_line(text), parse_line(QUALITY))
    assert m.valid and m.integration_ms == 0.5
    assert m.window * m.group_samples == 16
    validate_command("INTEGRATION 500US")
    validate_command("INTEGRATION 2MS")
