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

// --- ALTERAÇÃO: Pinos para o módulo de controle externo (NE555) ---
#define MOTOR_PWM_PIN 5  // Pino PWM para o sinal de velocidade
#define MOTOR_DIR_PIN 4  // Pino digital para o sinal de direção

// --- REMOVIDO: Pinos antigos do driver Ponte H ---
// const int RPWM = 5;
// const int LPWM = 6;
// const int L_EN = 9;
// const int R_EN = 10;

//-------------------- VARIÁVEIS --------------------
int status = 0;         // 0 Desligado - 1 Ligado
int velocidade = 0;     // ALTERAÇÃO: Agora em porcentagem (0-100)
int direcao = 0;        // 0 Frente - 1 Ré (pode ser necessário inverter dependendo do seu módulo)
int motordirecao = 0;   // 0 parado / 1 direita / 2 esquerda

// controle de “segurar botão”: atualiza enquanto chegam repetições
unsigned long lastDirSignalMillis = 0;
const unsigned long DIR_RELEASE_TIMEOUT = 150; // ms sem repetição = botão solto

//-------------------- CLASSE MOTOR DIREÇÃO (sem alterações) --------------------
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

//-------------------- FUNÇÕES AUXILIARES (REESCRITAS) --------------------
// Mantida, pois a frequência do Timer 0 (que afeta o pino 5) ainda é útil.
void setPWMfrequency(int freq) {
  TCCR0B = TCCR0B & 0b11111000 | freq;
}

// NOVA FUNÇÃO: Controla o motor com base na porcentagem e direção.
void controleMotor(int percentVelocidade, int direcaoMotor) {
  // Garante que a velocidade esteja nos limites de 0-100
  percentVelocidade = constrain(percentVelocidade, 0, 100);
  
  // Define a direção do motor
  // NOTA: você talvez precise trocar HIGH por LOW dependendo de como seu módulo interpreta o sinal
  digitalWrite(MOTOR_DIR_PIN, direcaoMotor == 0 ? LOW : HIGH); 

  // Mapeia a porcentagem (0-100) para o valor PWM (0-255)
  byte pwmValor = map(percentVelocidade, 0, 100, 0, 255);

  // Envia o pulso PWM para o pino de controle de velocidade
  analogWrite(MOTOR_PWM_PIN, pwmValor);
}

// NOVA FUNÇÃO: Para o motor de forma simples.
void paraMotor() {
  controleMotor(0, direcao); // Apenas seta a velocidade para 0, mantendo a direção atual
}


//-------------------- FUNÇÃO BIP (sem alterações) --------------------
void bip(int tp) {
  // ... (código da função bip permanece o mesmo)
  switch (tp) {
    case 1: analogWrite(BUZZER_PIN, 255); delay(2500); analogWrite(BUZZER_PIN, 0);   delay(700); analogWrite(BUZZER_PIN, 255); delay(700); analogWrite(BUZZER_PIN, 0);   delay(700); analogWrite(BUZZER_PIN, 255); delay(700); analogWrite(BUZZER_PIN, 0); break;
    case 2: analogWrite(BUZZER_PIN, 255); delay(4000); analogWrite(BUZZER_PIN, 0); break;
    case 3: analogWrite(BUZZER_PIN, 255); delay(800); analogWrite(BUZZER_PIN, 0);   delay(400); analogWrite(BUZZER_PIN, 255); delay(800); analogWrite(BUZZER_PIN, 0); break;
    case 4: for (int i = 0; i < 3; i++) { analogWrite(BUZZER_PIN, 255); delay(400); analogWrite(BUZZER_PIN, 0);   delay(400); } break;
    case 5: analogWrite(BUZZER_PIN, 255); delay(500); analogWrite(BUZZER_PIN, 0); break;
  }
}

//-------------------- FUNÇÃO DE PROCESSAMENTO DE COMANDOS (ATUALIZADA) --------------------
void processCommand(uint32_t code, bool isRepeat) {
  if (isRepeat) {
    if (motordirecao != 0) {
      lastDirSignalMillis = millis();
    }
    return;
  }

  switch (code) {
    // Botao Desligar/Liga
    case 0xE51A52AD:
      if (status == 0) {
        Serial.println("Ligado");
        bip(1);
        status = 1;
        velocidade = 10; // Inicia com 10%
        direcao = 0;     // Inicia para frente
        controleMotor(velocidade, direcao);
      } else {
        Serial.println("Desligado");
        bip(2);
        Motor1.Stop();
        motordirecao = 0;
        status = 0;
        velocidade = 0;
        paraMotor();
      }
      break;

    // Botao Velocidade 30%
    case 0xA75852AD:
      if (status == 1) {
        if (velocidade != 30) {
          Serial.println("Velocidade 30%");
          bip(5);
          velocidade = 30;
          controleMotor(velocidade, direcao);
        } else bip(4);
      }
      break;

    // Botao Frente
    case 0xBF4052AD:
      if (status == 1 && direcao == 1) {
        Serial.println("Frente");
        bip(5);
        paraMotor();      // Para antes de inverter
        delay(250);       // Pequena pausa para o motor assentar
        direcao = 0;
        velocidade = 20;  // Velocidade inicial ao mudar de direção
        controleMotor(velocidade, direcao);
      }
      break;

    // Botao Ré
    case 0xBE4152AD:
      if (status == 1 && direcao == 0) {
        Serial.println("Ré");
        bip(5);
        paraMotor();      // Para antes de inverter
        delay(250);       // Pequena pausa
        direcao = 1;
        velocidade = 20;  // Velocidade inicial ao mudar de direção
        controleMotor(velocidade, direcao);
      }
      break;

    // Botao + Velocidade
    case 0xF50A52AD:
      if (status == 1) {
        if (velocidade < 100) {
          Serial.println("+ Velocidade");
          bip(5);
          velocidade += 10; // Aumenta em passos de 10%
          if (velocidade > 100) velocidade = 100; // Trava em 100%
          controleMotor(velocidade, direcao);
        } else {
          Serial.println("!Velocidade Maxima já!");
          bip(4);
        }
      }
      break;

    // Botao - Velocidade
    case 0xF40B52AD:
      if (status == 1) {
        if (velocidade > 0) {
          Serial.println("- Velocidade");
          bip(5);
          velocidade -= 10; // Diminui em passos de 10%
          if (velocidade < 0) velocidade = 0; // Trava em 0%
          controleMotor(velocidade, direcao);
        } else {
          Serial.println("!Velocidade Minima já!");
          bip(4);
        }
      }
      break;
      
    // Comandos do motor de direção (sem alteração)
    case 0xBC4352AD: if (status == 1) { motordirecao = 1; Motor1.Forward(); Serial.println("Direita"); bip(5); lastDirSignalMillis = millis();} break;
    case 0xBD4252AD: if (status == 1) { motordirecao = 2; Motor1.Backward(); Serial.println("Esquerda"); bip(5); lastDirSignalMillis = millis();} break;
  }
}

//-------------------- SETUP (ATUALIZADO) --------------------
void setup() {
  setPWMfrequency(0x02); // timer 0, 3.92KHz (mantido)
  Serial.begin(9600);
  IrReceiver.begin(12);

  Motor1.Pinout(3, 11);
  Motor1.Speed(100);

  // --- ALTERAÇÃO: Configura os novos pinos do motor de propulsão ---
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  digitalWrite(MOTOR_DIR_PIN, LOW); // Começa com direção para frente
  analogWrite(MOTOR_PWM_PIN, 0);    // Começa com motor parado

  pinMode(BUZZER_PIN, OUTPUT);
  bip(3);
}

//-------------------- LOOP (sem alterações) --------------------
void loop() {
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    bool isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) || (code == 0xFFFFFFFF);
    processCommand(code, isRepeat);
    IrReceiver.resume();
  }

  if (motordirecao != 0 && (millis() - lastDirSignalMillis) > DIR_RELEASE_TIMEOUT) {
    Motor1.Stop();
    motordirecao = 0;
  }
}