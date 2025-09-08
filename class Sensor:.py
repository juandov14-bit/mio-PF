class Sensor:
    def __init__(self, name, value=0.0, unit=""):
        self.name = name
        self.value = value
        self.unit = unit

    def read(self):
        # Simulate reading a sensor value
        return self.value

    def set_value(self, value):
        self.value = value

    def __repr__(self):
        return f"<Sensor name={self.name} value={self.value}{self.unit}>"
