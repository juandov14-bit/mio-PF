class Sensor:
    def __init__(self, nombre, valor_inicial):
        self.nombre = nombre
        self.valor = valor_inicial
        self.valor_anterior = valor_inicial

    def actualizar_valor(self, nuevo_valor):
        self.valor_anterior = self.valor
        self.valor = nuevo_valor

    def getGradient(self):
        if self.valor > self.valor_anterior:
            return 1
        elif self.valor < self.valor_anterior:
            return -1
        else:
            return 0


# ejemplo de uso
s = Sensor("TempSensor1", 25)

s.actualizar_valor(27)
print(s.getGradient())  # 1 (subió)

s.actualizar_valor(23)
print(s.getGradient())  # -1 (bajó)

s.actualizar_valor(23)
print(s.getGradient())  # 0 (igual)


































































