//INCLUDES PARA OTA
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//DEFINICIONES PARA OTA
const char* ssid = "red_programacion";
const char* password = "0123456789";

bool OTA = false; //indicador de si se está en modo OTA
/////////////////////////////////////////

//////////////////////////
//Definiciones para los GPIO

#define pinRele   19 //pin para activar el relé//
#define pinLED    32 //pin de LED indicador (VERDE)
#define pinReedSw 33 //pin de entrada Reed Switch



//Horas que transcurriran entre reinicios de WW
#define hrspReinicio 3



//Definiciones para parpadeo de LED

#define LedOpNormal 1000000 //Parpadeo de 1Hz
#define LedOpTrans  1       //Encendido continuo
#define LedOpOTA    50000   //Parpadeo rápido de 20Hz



////////////////////////////////////////
/*Variables para los Hardware Timers  */
hw_timer_t *tReinicio = NULL; //timer para contar el tiempo hasta el reinicio
hw_timer_t *tLED = NULL; //Timer para controlar comportamiento de LED indicador 
hw_timer_t *tWD = NULL; //Watchdog para rutina de programación OTA (entrar y salir de ella)

/////////////////////////////////////////
unsigned long horasReinicio = 0; //contador de las horas de reinicio


bool reinicioInt = false; //variable para habilitar rutina de reinciio en ww

////////////////////////////////////////
/*Variables relacionadas a la operacion de los timers */

byte contSegundos = 0; //variable que cuenta los segundos para entrar a modo OTA

/////////////////////////////////////////



void setup(){

  //Inicializamos GPIOs
  confGPIOs();

  //Inicializamos timers
  timersInit();

}


void loop(){


  //Si entramos en modo OTA
  if(OTA){

    ModoOTA();

  }


  if(reinicioInt ){


    reinicioInt = false;


    /*timerWrite(tAux, 32000000); //cargamos registro del timer con 32 segundos 
    * timerStart(tAux); //empezamos cuenta*/

    //Encendemos rele
    digitalWrite(pinRele,1);

    //esperamos 31 segundos
    delay(31000);


    //Apagamos rele
    digitalWrite(pinRele,0);


  
  }


}

//Subrutina para configurar entradas y salidas
void confGPIOs(){

  //Configuramos el relé como salida
  pinMode(pinRele,OUTPUT);
  //Apagamos relé
  digitalWrite(pinRele,0);


  //Configuramos LED indicador como salida 
  pinMode(pinLED,OUTPUT);
  //Apagamos el LED 
  digitalWrite(pinLED,0);

  //Configuramos como entrada el reed switch con resistencia pull-up
  pinMode(pinReedSw,INPUT_PULLUP);


  //Activamos interrupcióin a GPIO de reed switch
  attachInterrupt(pinReedSw, ISRReedSw, CHANGE);

}



void ISRReedSw(){
  //esta interrupción se "dispara" con cualquier cambio en el flanco

  //primero verificamos si la interrupción se disparo para entrar a modo ota
  //en ese casoe l contador de segundos esta en 0
  if(!OTA){
    //Si estamos empezando  a  entrar en modo OTA activamos timer 

    if(digitalRead(pinReedSw) == 0){
      //significa que el imán está presente
      timerRestart(tWD);// Reiniciamos la cuenta del timer a 0
      timerStart(tWD);
      timerAlarmEnable(tWD);

      //Apagamos timer del LED y lo dejamos "encendido"
      timerStop(tLED);
      digitalWrite(pinLED,LedOpTrans);
    }else{
      //el imán no está presente
      timerRestart(tLED);
      timerStart(tLED);
      timerStop(tWD);
    }



  }else{
    //si ya activamos el modo OTA 



  }


}





void timersInit(){

  tReinicio = timerBegin(0, 80, true);//HW timer 0, divisor de 80, cuenta arriba
  tLED = timerBegin(1, 80, true); //HW timer 1, divisor de 80, cuenta arriba
  tWD = timerBegin(2, 80, true); //HW timer 2, divisor de 80, cuenta arriba

  timerAttachInterrupt(tReinicio, &ISRtReinicio, true);//configuramos INT del reinicio periodico
  timerAttachInterrupt(tLED, &ISRtLED, true);//configuramos INT del reinicio periodico
  timerAttachInterrupt(tWD, &ISRtWatchDogOTA, true);//configuramos INT del modo OTA

  //Timer de alarma
  timerAlarmWrite(tReinicio, 3600000000, true); // configuramos el tiempo para reinicio (1 hr)
  timerAlarmEnable(tReinicio); //Habilitamos timer 

  //Timer de parpadeo de LED
  timerAlarmWrite(tLED, 1000000, true); // parpadeo normal led de 1s
  timerAlarmEnable(tLED); //Habilitamos timer 

  //Timer de confirmacion de entarar a modo OTA
  timerAlarmWrite(tWD, 3000000, true); // parpadeo normal led de 1s


  
} 



void ISRtReinicio(){
  //subrutina para reinicio periodico de WattWatcher

  //cada que se llame esta subrutina habilitaremos el reinicio del wattwathcer
  horasReinicio++;

  if(horasReinicio == hrspReinicio){

    horasReinicio = 0;
    reinicioInt = true;

  }
  
  

}

void ISRtWatchDogOTA(){
  //Esta ISR se encarga de validar que se cumplio
  //el tiempo requerido de reed switch para entrar en modo OTA
  //y para sacarlo en caso de que pase mucho tiempo

  if(OTA){
    //Si estamos en modo ota cuando se mande a llamar esta interrupción debemos de salir de ahí
    //En esta version se hara un reinicio del sistema 
    ESP.restart();
  }else{
    //si estamos contando los segundos para activar el modo OTA
    //al llegar a esta interrupcion activamos la bandera 
    if(digitalRead(pinReedSw) == 0){
      //entonces significa que se mantuvo el tiempo necesario
      OTA = true; //Activamos modo OTA
      timerAlarmWrite(tWD, 1000000*60*5, true); // watchdog de 5 mins
      timerAlarmWrite(tLED, LedOpOTA, true); // parpadeo de LED indicando modo OTA
      timerRestart(tLED);
      timerStart(tLED);
      timerRestart(tWD);
      timerStart(tWD);
      timerAlarmEnable(tLED); //Habilitamos time


    }
    


  }
}

void ISRtLED(){
  //Solo se cambia el estado del LED para que parpadee
  digitalWrite(pinLED,!digitalRead(pinLED));
}

void ModoOTA(){

  //Funcion para configurar y entrar en modo OTA
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    //esperamos 5s para intentar reconexión    
    delay(5000);
  }
  
  ArduinoOTA.setPort(3232);

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    });
    
  ArduinoOTA.begin();

  while(1){
    ArduinoOTA.handle();
  }



}





















