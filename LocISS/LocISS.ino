#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

#define LED      15

// Conexões ao MAX7219
#define PIN_CLK  14
#define PIN_LOAD 16
#define PIN_DIN  2

// Registradores do MAX7219
#define MAX7219_NOP   0x00
#define MAX7219_DIG0  0x01
#define MAX7219_DIG1  0x02
#define MAX7219_DIG2  0x03
#define MAX7219_DIG3  0x04
#define MAX7219_DIG4  0x05
#define MAX7219_DIG5  0x06
#define MAX7219_DIG6  0x07
#define MAX7219_DIG7  0x08
#define MAX7219_MODE  0x09
#define MAX7219_INT   0x0A
#define MAX7219_LIM   0x0B
#define MAX7219_SHUT  0x0C
#define MAX7219_TEST  0x0F

// CONEXOES DOS LEDS
#define DIG1   MAX7219_DIG7
#define DIG2   MAX7219_DIG6
#define DIG3   MAX7219_DIG5
#define DIG4   MAX7219_DIG4
#define DIG5   MAX7219_DIG3
#define DIG6   MAX7219_DIG2
#define DIG7   MAX7219_DIG1
#define DIG8   MAX7219_DIG0

// Para determinar o valor a escrever
const uint8_t digito[] = { DIG1, DIG2, DIG3, DIG4, DIG5, DIG6, DIG7, DIG8 };
uint8_t valor [3][8];

// Coloque aqui os dados do seu roteador
struct {
  char *ssid;
  char *passwd;
} redes[] = {
  { "AAAAAA", "segredo1" },
  { "BBBBBB", "segredo2" },
  { NULL, NULL }  
};

// Acesso à api
const char *hostApi = "garoa.net.br";
const uint16_t portApi = 80;
const char *urlPos = "/iss/position";
const char *urlPas = "/iss/passage";

WiFiClient client;
HTTPClient http;

// Temporizações
const unsigned long tempPos = 5000; // Atualiza posição a cada 5seg
os_timer_t timerSeg;
bool zerou = true;

/*
 * Exemplos de resposta
 * 
 * Posição:
 * "-27.38857383860919|-81.43535823804964|416566"
 * 
 * Passagem:
 * "0|0|11|47|46"
 * 
 */

// Iniciação
void setup ()
{
  // Configura os pinos
  pinMode (LED, OUTPUT);
  digitalWrite (LED, HIGH);
  pinMode (PIN_LOAD, OUTPUT);
  pinMode (PIN_DIN, OUTPUT);
  pinMode (PIN_CLK, OUTPUT);
  digitalWrite (PIN_LOAD, HIGH);
  digitalWrite (PIN_CLK, LOW);
  digitalWrite (PIN_DIN, LOW);

  // Configura o MAX7219
  write7219 (MAX7219_SHUT, 0x00);
  write7219 (MAX7219_TEST, 0x00);
  write7219 (MAX7219_MODE, 0xFF);
  write7219 (MAX7219_INT, 0x0F);
  write7219 (MAX7219_LIM, 0x07);

  // Iniciar apresentando zeros
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 8; j++)
      valor[i][j] = 0;
  atlDisplay();
  write7219 (MAX7219_SHUT, 0x01);

  // Conectar ao WiFi
  Serial.begin (115200);
  int iRede = -1;
  WiFi.mode (WIFI_STA);
  while (iRede == -1) {
    WiFi.disconnect ();
    delay (2000);
    int n = WiFi.scanNetworks();
    for (int i = 0; (iRede == -1) && (i < n); i++) {
      for (int j = 0; redes[j].ssid != NULL; j++) {
        if (strcmp (redes[j].ssid, WiFi.SSID(i).c_str()) == 0) {
          iRede = j;
          break;
        }
      }
    }
  }
  Serial.print ("Conectando a ");
  Serial.println (redes[iRede].ssid);
  WiFi.begin(redes[iRede].ssid, redes[iRede].passwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay (100);
  }
  Serial.println ("Conectado.");

  // Timer para atualizar tempo
  os_timer_setfn(&timerSeg, timerCallback, NULL);
  os_timer_arm(&timerSeg, 1000, true);
}

// Laco principal
void loop ()
{
  static unsigned long proxpos = millis() + tempPos;
  
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() >= proxpos) {
      // Consulta posição atual
      if (!http.begin(client, hostApi, portApi, urlPos)) {
        Serial.println('Não conectou?');
      }
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        String resposta = http.getString();
        //Serial.println(resposta);
        trataPosicao(resposta.c_str());
      } else {
        Serial.println(httpCode);
      }
      http.end();
      do {
        proxpos = millis() + tempPos;
      } while (proxpos < millis());
    }

    if (zerou) {
      // Atualizar informação de passagem
      Serial.println ("Atualiza passagem.");
      http.begin(client, hostApi, portApi, urlPas);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        String resposta = http.getString();
        trataPassagem(resposta.c_str());
      }
      http.end();
    }
  }
  atlDisplay();
  delay (200);
}

// Extrai a posição da resposta e mostra no display
void trataPosicao(const char *resp) {
  double latitude, longitude;
  sscanf(resp+1, "%lf|%lf|", &latitude, &longitude);
  fmtval (2, (long) (latitude * 10000.0));
  fmtval (1, (long) (longitude * 10000.0));
}

// Extrai a informação de passagem
void trataPassagem(const char *resp) {
  int visivel, dias, horas, minutos, segundos;
  sscanf(resp+1, "%d|%d|%d|%d|%d", &visivel, &dias, &horas, 
        &minutos, &segundos);
  digitalWrite (LED, visivel ? HIGH : LOW);
  os_timer_disarm(&timerSeg);
  valor[0][0] = (dias / 10) % 10;
  valor[0][1] = (dias % 10) | 0x80;
  valor[0][2] = horas / 10;
  valor[0][3] = (horas % 10) | 0x80;
  valor[0][4] = minutos / 10;
  valor[0][5] = (minutos % 10) | 0x80;
  valor[0][6] = segundos / 10;
  valor[0][7] = segundos % 10;
  int tmpPas = 0;
  for (int i = 0; i < 8; i++) {
    tmpPas += (valor[0][i] & 0x7F);
  }
  zerou = tmpPas == 0;
  os_timer_arm(&timerSeg, 1000, true);
}

// Formata o valor para apresentação no display
// -999.9999
void fmtval (int disp, long val) {
  if (val < 0L) {
    // Apresentar '-' na frente se negativo
    val = -val;
    valor[disp][0] =  10;
  } else {
    valor[disp][0] =  15;
  }
  for (int i = 7; i > 0; i--) {
    valor[disp][i] = val % 10L;
    val = val / 10L;
  }
  // Coloca o ponto
  valor[disp][3] |= 0x80;
}

// Atualiza o display
void atlDisplay(void) {
  for (int i = 0; i < 8; i++) {
    write7219b (digito[i], valor[0][i], valor[1][i], valor[2][i]);
  }
}

// Rotina chamada a cada 1 segundo para atualizar tempo
void timerCallback(void *pArg) {
  if (!zerou) {
    if (valor[0][7] != 0) {
      valor[0][7]--;
    } else {
      valor[0][7] = 9;
      if (valor[0][6] != 0) {
        valor[0][6]--;
      } else {
        valor[0][6] = 5;
        if (valor[0][5] != 0x80) {
          valor[0][5]--;
        } else {
          valor[0][5] = 0x89;
          if (valor[0][4] != 0) {
            valor[0][4]--;
          } else {
            valor[0][4]= 5;
            if (valor[0][3] != 0x80) {
              valor[0][3]--;
            } else {
              valor[0][3] = 0x89;
              if (valor[0][2] != 0) {
                valor[0][2]--;
              } else {
                valor[0][2] = 2;
                valor[0][3] = 0x83;
                if (valor[0][1] != 0x80) {
                  valor[0][1]--;
                } else {
                  valor[0][1] = 0x89;
                  valor[0][0]--;
                }
              }
            }
          }
        }
      }
    }
    int tmpPas = 0;
    for (int i = 0; i < 8; i++) {
      tmpPas += (valor[0][i] & 0x7F);
    }
    zerou = tmpPas == 0;
  }
}


// Escreve o mesmo valor no mesmo registrador nos três módulos
void write7219 (uint8_t addr, uint8_t data)
{
  digitalWrite (PIN_LOAD, LOW);
  for (int i = 0; i < 3; i++) {
    shiftOut (PIN_DIN, PIN_CLK, MSBFIRST, addr);
    shiftOut (PIN_DIN, PIN_CLK, MSBFIRST, data);
  }
  digitalWrite (PIN_LOAD, HIGH);
}

// Escreve valores diferentes no mesmo registrador nos três módulos
void write7219b (uint8_t addr, uint8_t data1, uint8_t data2, uint8_t data3)
{
  digitalWrite (PIN_LOAD, LOW);
  shiftOut (PIN_DIN, PIN_CLK, MSBFIRST, addr);
  shiftOut (PIN_DIN, PIN_CLK, MSBFIRST, data1);
  shiftOut (PIN_DIN, PIN_CLK, MSBFIRST, addr);
  shiftOut (PIN_DIN, PIN_CLK, MSBFIRST, data2);
  shiftOut (PIN_DIN, PIN_CLK, MSBFIRST, addr);
  shiftOut (PIN_DIN, PIN_CLK, MSBFIRST, data3);
  digitalWrite (PIN_LOAD, HIGH);
}
