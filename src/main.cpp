#include <Arduino.h>
#include <IRremote.h> // Versão 3.x ou superior

/*
MAPEAMENTO CONTROLE HEX
  POWER (Botao Desligar/Liga) = 0xE51A52AD
  PLAY/PAUSE (Botao Velocidade 30%) = 0xA75852AD
  ESQUERDA (Botao Direcao > Esquerda) = 0xBD4252AD
  DIREITA (Botao Direcao > Direita) = 0xBC4352AD
  MAIS (Botao + Velocidade) = 0xF50A52AD
  MENOS (Botao - Velocidade) = 0xF40B52AD
  PASTA MAIS (Botao Frente) = 0xBF4052AD
  PASTA MENOS (Botao Ré) = 0xBE4152AD
*/

//-------------------- PINOS --------------------
#define BUZZER_PIN 2
#define MOTOR_PWM_PIN     5  // Pino PWM para o sinal de velocidade
#define MOTOR_DIR_FRENTE  4  // Pino digital para o sinal de direção FRENTE
#define MOTOR_DIR_RE      7  // Pino digital para o sinal de direção RÉ

//-------------------- VARIÁVEIS --------------------
int status = 0;         // 0 Desligado - 1 Ligado
int velocidade = 0;     // 0-100 %
int direcao = 0;        // 0 Frente - 1 Ré
int motordirecao = 0;   // 0 parado / 1 direita / 2 esquerda

// Controle de “segurar botão”: atualiza enquanto chegam repetições
unsigned long lastDirSignalMillis = 0;
const unsigned long DIR_RELEASE_TIMEOUT = 150; // ms sem repetição = botão solto

// Controle de tempo mínimo entre trocas de direção
unsigned long lastDirectionChangeMillis = 0;
const unsigned long DIRECTION_CHANGE_DELAY = 500; // tempo mínimo entre trocas (ms)

//-------------------- CLASSE MOTOR DIREÇÃO --------------------
class DCMotor {
  int spd = 130, pin1, pin2;
public:
  void Pinout(int in1, int in2) {
    pin1 = in1;
    pin2 = in2;
    pinMode(pin1, OUTPUT);
    pinMode(pin2, OUTPUT);
  }
  void Speed(int in1) { spd = in1; }
  void Forward() {
    digitalWrite(pin1, HIGH);
    digitalWrite(pin2, LOW);
  }
  void Backward() {
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, HIGH);
  }
  void Stop() {
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
  }
};
DCMotor Motor1;

//-------------------- FUNÇÕES AUXILIARES --------------------
void setPWMfrequency(int freq) {
  TCCR0B = TCCR0B & 0b11111000 | freq;
}

// Controla motor principal (com trava de segurança)
void controleMotor(int percentVelocidade, int direcaoMotor) {
  percentVelocidade = constrain(percentVelocidade, 0, 100);

  if (percentVelocidade == 0) {
    digitalWrite(MOTOR_DIR_FRENTE, LOW);
    digitalWrite(MOTOR_DIR_RE, LOW);
  } 
  else if (direcaoMotor == 0) { // Frente
    digitalWrite(MOTOR_DIR_FRENTE, HIGH);
    digitalWrite(MOTOR_DIR_RE, LOW);
  } 
  else { // Ré
    digitalWrite(MOTOR_DIR_FRENTE, LOW);
    digitalWrite(MOTOR_DIR_RE, HIGH);
  }

  byte pwmValor = map(percentVelocidade, 0, 100, 0, 255);
  analogWrite(MOTOR_PWM_PIN, pwmValor);
}

// Para motor com pausa de segurança
void pararComSeguranca() {
  controleMotor(0, direcao);
  delay(2000); // Pausa física para proteger
  lastDirectionChangeMillis = millis();
}

void paraMotor() {
  controleMotor(0, direcao);
}

//-------------------- FUNÇÃO BIP --------------------
void bip(int tp) {
  switch (tp) {
    case 1: analogWrite(BUZZER_PIN, 255); delay(2500); analogWrite(BUZZER_PIN, 0); delay(700);
            analogWrite(BUZZER_PIN, 255); delay(700); analogWrite(BUZZER_PIN, 0); delay(700);
            analogWrite(BUZZER_PIN, 255); delay(700); analogWrite(BUZZER_PIN, 0); break;
    case 2: analogWrite(BUZZER_PIN, 255); delay(4000); analogWrite(BUZZER_PIN, 0); break;
    case 3: analogWrite(BUZZER_PIN, 255); delay(800); analogWrite(BUZZER_PIN, 0); delay(400);
            analogWrite(BUZZER_PIN, 255); delay(800); analogWrite(BUZZER_PIN, 0); break;
    case 4: for (int i = 0; i < 3; i++) { analogWrite(BUZZER_PIN, 255); delay(400);
            analogWrite(BUZZER_PIN, 0); delay(400); } break;
    case 5: analogWrite(BUZZER_PIN, 255); delay(500); analogWrite(BUZZER_PIN, 0); break;
  }
}

//-------------------- PROCESSAMENTO COMANDOS --------------------
void processCommand(uint32_t code, bool isRepeat) {
  // ---------------- REPETIÇÃO DE BOTÃO ----------------
  if (isRepeat) {
    if (motordirecao != 0) {
      lastDirSignalMillis = millis();

      // Mantém motor de direção ativo enquanto botão está pressionado
      if (motordirecao == 1) {
        Motor1.Forward();
      } else if (motordirecao == 2) {
        Motor1.Backward();
      }
    }
    return;
  }

  // ---------------- COMANDOS NORMAIS ----------------
  switch (code) {
    case 0xE51A52AD: // Liga/Desliga
      if (status == 0) {
        bip(1);
        status = 1;
        velocidade = 15;
        direcao = 0;
        controleMotor(velocidade, direcao);
      } else {
        bip(2);
        Motor1.Stop();
        motordirecao = 0;
        status = 0;
        velocidade = 0;
        paraMotor();
      }
      break;

    case 0xA75852AD: // Velocidade 30%
      if (status == 1) {
        if (velocidade != 30) {
          bip(5);
          velocidade = 30;
          controleMotor(velocidade, direcao);
        } else bip(4);
      }
      break;

    case 0xBF4052AD: // Frente
      if (status == 1 && direcao == 1) {
        if (millis() - lastDirectionChangeMillis >= DIRECTION_CHANGE_DELAY) {
          bip(5);
          pararComSeguranca();
          direcao = 0;
          velocidade = 15;
          controleMotor(velocidade, direcao);
        } else {
          bip(4);
        }
      }
      break;

    case 0xBE4152AD: // Ré
      if (status == 1 && direcao == 0) {
        if (millis() - lastDirectionChangeMillis >= DIRECTION_CHANGE_DELAY) {
          bip(5);
          pararComSeguranca();
          direcao = 1;
          velocidade = 15;
          controleMotor(velocidade, direcao);
        } else {
          bip(4);
        }
      }
      break;

    case 0xF50A52AD: // + Velocidade
      if (status == 1) {
        if (velocidade < 100) {
          bip(5);
          velocidade += 10;
          if (velocidade > 100) velocidade = 100;
          controleMotor(velocidade, direcao);
        } else bip(4);
      }
      break;

    case 0xF40B52AD: // - Velocidade
      if (status == 1) {
        if (velocidade > 0) {
          bip(5);
          velocidade -= 10;
          if (velocidade < 0) velocidade = 0;
          controleMotor(velocidade, direcao);
        } else bip(4);
      }
      break;

    case 0xBC4352AD: // Direita
      if (status == 1) {
        motordirecao = 1;
        Motor1.Forward();
        bip(5);
        lastDirSignalMillis = millis();
      }
      break;

    case 0xBD4252AD: // Esquerda
      if (status == 1) {
        motordirecao = 2;
        Motor1.Backward();
        bip(5);
        lastDirSignalMillis = millis();
      }
      break;
  }
}

//-------------------- SETUP --------------------
void setup() {
  setPWMfrequency(0x02); // timer 0, 3.92KHz
  Serial.begin(9600);
  IrReceiver.begin(12);

  Motor1.Pinout(3, 11);
  Motor1.Speed(100);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_FRENTE, OUTPUT);
  pinMode(MOTOR_DIR_RE, OUTPUT);
  digitalWrite(MOTOR_DIR_FRENTE, LOW);
  digitalWrite(MOTOR_DIR_RE, LOW);
  analogWrite(MOTOR_PWM_PIN, 0);

  pinMode(BUZZER_PIN, OUTPUT);
  bip(3);
}

//-------------------- LOOP --------------------
void loop() {
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    bool isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) || (code == 0xFFFFFFFF);
    processCommand(code, isRepeat);
    IrReceiver.resume();
  }

  // Soltou botão de direção → para motor
  if (motordirecao != 0 && (millis() - lastDirSignalMillis) > DIR_RELEASE_TIMEOUT) {
    Motor1.Stop();
    motordirecao = 0;
  }
}
