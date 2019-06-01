import asyncio


class Timer:
    """Scheduling periodic callbacks"""
    def __init__(self, interval, callback):
        self.interval = interval
        self.callback = callback
        self.loop = asyncio.get_event_loop()

        self.is_active = False

    def start(self):
        self.is_active = True
        self.handler = self.loop.call_later(self.get_interval(), self._run)

    def _run(self):
        if self.is_active:
            self.callback()
            self.handler = self.loop.call_later(self.get_interval(), self._run)

    def stop(self):
        self.is_active = False
        self.handler.cancel()

    def reset(self):
        self.stop()
        self.start()

    def get_interval(self):
        return self.interval() if callable(self.interval) else self.interval
