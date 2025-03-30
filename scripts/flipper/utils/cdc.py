import glob
import serial.tools.list_ports as list_ports


# Returns a valid port or None, if it cannot be found
def resolve_port(logger, portname: str = "auto"):
    if portname != "auto":
        return portname
    # Try guessing
    flippers = list(list_ports.grep("flip_"))
    if len(flippers) == 1:
        flipper = flippers[0]
        logger.info(f"Using {flipper.serial_number} on {flipper.device}")
        return flipper.device
    elif len(flippers) == 0:
        # Fallback: check for tty_flip_{serial} devices
        tty_flip_devices = glob.glob("/dev/tty_flip_*")
        if tty_flip_devices:
            device_path = tty_flip_devices[0]
            # Extract serial from path
            serial = device_path.split("_")[-1]
            logger.info(
                f"Fallback: Found Flipper device with serial {serial} at {device_path}"
            )
            return device_path
        logger.error("Failed to find connected Flipper")
    elif len(flippers) > 1:
        logger.error("More than one Flipper is attached")
