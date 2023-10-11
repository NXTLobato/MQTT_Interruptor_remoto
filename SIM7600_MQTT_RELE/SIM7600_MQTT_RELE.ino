
//////////////////////////
//Definiciones para los puertos cereales
//MQTT_0
#define RXD1 16 //pin RX para serial del modem
#define TXD1 17 //pin TX para serial del modem
#define pinRele 19 //pin para activar el relé//

/*/////////////////////////
* ESP32THING GPIO 0 btn con PULLUP
* NODEMCU32S ??????
* FEATHER32  SIN BOTON INTEGRADO
*/
//#define pinBtn 18 //pin para boton integrado en tarjeta

#define INTENTOS 3 //Maximo numero de intentos

//Definiciones para el MQTT
/////////////////////////////////////////////////////////
#define MQTT_HOST   "broker.losant.com" //Broker de losant
#define MQTT_PUERTO "1883" //Puerto del broker

/////////////////////
//Parametros de MQTT
#define MQTT_ID   "64cabae015e67ca2479c7b70" //Identificador del device registrado
#define MQTT_USER "8ac61884-2b38-441d-894e-2b5a80453dce" //Usuario (ACCESS KEY de Losant)
#define MQTT_PASS "7e513642a8d745f514244ee4ee8d805b56e60805a54a1fee5807207e630ec166" //Contraseña (ACCESS SECRET de Losant)
/////////////////////

//TEMA DE SUSCRIPCION
#define MQTT_SUBTOPIC "SIM7600/TEST/SUB"

//TEMA DE PUBLICACION
#define MQTT_DEVICEPUB "SIM7600/TEST/PUB"


//Se definen comandos para funcionalidad
#define MQTT_REINICIO "NXTCODEON" //Cadena a buscar para efectuar un reinicio


//AQUI ESTA EL PARAMETRO DE REINICIO
#define hrspReinicio 24

//Definiciones para timers
/////////////////////////////////////////////////////////
#define hrsReinicio 3600  //Dias expresadas en horas para reinicio periodico


/////////////////////////////////////////////////////////

//Definiciones para logica de firmware
/////////////////////////////////////////////////////////
bool reinicioSerie = false; //Variable que define si hay que reiniciar (desde puerto serie=)
bool reinicioInt = false; //variable que define si hay reinicio (desde ISR de timer)
bool habReconexion = false; //variable que permite o no hacer un intento de reconexión a MQTT

bool MQTTconectado = false; //variable para saber si estamos conectados a broker de losant

/////////////////////////////////////////////////////////
String modemAT = ""; //Variable que captura el comando AT del modem

////////////////////////////////////////
/*Variables para los Hardware Timers  */
hw_timer_t *tReinicio = NULL; //timer para contar el tiempo hasta el reinicio
hw_timer_t *tAux = NULL; //Timer para diversas funciones
hw_timer_t *tMonitoreo = NULL; //Timer periodico que habilita la reconexion a MQTT

/////////////////////////////////////////
byte horasReinicio = 0; //contador de las horas de reinicio

//variables parametricas para configurar funcionalidad
int cfgDias = 15; //Dias que deben transcurrir para reinicio periodico

int cfgHora = 12; //Hora en formato de 24 horas para realizar el reinicio
int cfgMin  = 0; //Minuto en el que se debe realizar el reinicio

bool habPING = false; //variable para habilitar o no un mensaje PING

void setup() {
  // put your setup code here, to run once:
  delay(5000);
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1);


  pinMode(pinRele,OUTPUT);
  digitalWrite(pinRele,0);

  //Inicializamos el GPIO del botton integrado (si es que está disponible)
  //pinMode(pinBtn,INPUT);

  timersInit();


  //Asignacion de int
  //attachInterrupt(pinBtn, ISRbtn, FALLING);

  Serial.println("Empezando Warmup");
  mdmWarmUp();
  

  Serial.println("Inicializando MQTT");
  byte resultado = initMQTT(0);
  byte intRes = 0; //intentos restantes para ejecutar la misma subrutina

  while((resultado < 8) && (intRes != INTENTOS)){
    delay(60000); //esperamos 1 minuto a reintentar
    intRes++;
    resultado = initMQTT(resultado);
  }

  //al salir si intRes es igual a INTENTOS se reinicia el ESP32
  if(intRes == INTENTOS){
    ESP.restart();
  }else{
    intRes = 0; //Si no reiniciamos variable
  }
  
  delay(1000);

  Serial.println("Iniciando Suscripcion");

  while((!subMQTT(MQTT_SUBTOPIC)) && (intRes != INTENTOS)){
    delay(60000); //esperamos 1 minuto a reintentar
    intRes++;
  }

  //al salir si intRes es igual a INTENTOS se reinicia el ESP32
  if(intRes == INTENTOS){
    ESP.restart();
  }else{
    intRes = 0; //Si no reiniciamos variable
  }

  
  Serial.println("Listo");

  MQTTconectado = true;

  Serial1.println("AT+CSQ");
  Serial1.find("+CSQ: ");
  String senial = Serial1.readStringUntil(',');
  pubMQTT("{\"estado\":\"INICIO\",\"senial\":"+senial+"}");

  Serial1.readString(); //Limpieza del buffer

  




}


void loop(){

  /*if(desMQTT){
    desMQTT = false;
    disMQTT();
  }*/

  

  if(Serial1.available() > 0){
    //Significa que el modem recibio algo, se decodifica qué es 

    Serial1.find("+"); //Nos ponemos al inicio de la respuesta del modem 

    modemAT = Serial1.readString();

    Serial.println();
    Serial.println(modemAT);

    //if(modemAT == "CMQTTRXSTART"){
    if(true){
      //Entonces se trata de un payload por parte del HOST

      //hasta el momento el unico payload implica que se requiere un reinicio remoto del dispositivo conectado

      //Se agrega la rutina de reiniciar por 30 + 1 segundos lo que este conectado
      //al terminar se 'limpia' el buffer
      if(Serial1.find(MQTT_REINICIO)){
      
      
      reinicioSerie = true; //indicamos que debe de hacerse un reinicio


      }else{
        //APAGAMOS RELE
        digitalWrite(pinRele,LOW);

      }
      //limpiamos buffer
      Serial1.readString();


    }else if(Serial1.find("CMQTTNONET")){
      //Vamos a buscar entonces si hay un +CMQTTNONET que indique desconexión del servicio
      Serial1.readString(); //LImpieza de buffer

      Serial.println("Se perdio la conexion a MQTT se reintentará la conexión"); 
      
      MQTTconectado = false; //Indicamos que no estamos conectados

      
  
    }
  }


  //seccion para envios de ping constantes
  if(habPING){

    habPING = false;
    Serial1.println("AT+CSQ");
    Serial1.find("+CSQ: ");
    String senial = Serial1.readStringUntil(',');
    pubMQTT("{\"estado\":\"PING\",\"senial\":"+senial+"}");

    Serial1.readString(); //Limpieza del buffer

  }
  //Seccion que se encarga de ver si es que se debe hacer un reinicio al WattWatcher
  if(reinicioSerie || reinicioInt ){
    //Si cualquiera de los dos triggers indica reinicio de wattWatcher se reinicia
    
    if(reinicioInt){
      Serial.println("Reinicio por INT");
    }

    if(reinicioSerie){
      Serial.println("Reinicio por MQTT");
    }

    

    reinicioSerie = false;
    reinicioInt = false;


    /*timerWrite(tAux, 32000000); //cargamos registro del timer con 32 segundos 
    * timerStart(tAux); //empezamos cuenta*/

    //Encendemos rele
    digitalWrite(pinRele,1);

    delay(31000);
    /* /esperamos 30s
    while(timerRead(tAux) > 10000){

    }
    */

    //Encendemos rele
    digitalWrite(pinRele,0);


    
    Serial1.println("AT+CSQ");
    Serial1.find("+CSQ: ");
    String senial = Serial1.readStringUntil(',');
    pubMQTT("{\"estado\":\"Reinico de WW\",\"senial\":"+senial+"}");

    Serial1.readString(); //Limpieza del buffer
    
    

    //timerStop(tAux);
    






  }

  //Seccion que se encarga de hacer la reconexion del dispositivo al MQTT
  if(!MQTTconectado && habReconexion){
    //Si estamos desconectados...

    /*
    * Se le da un reinicio al modem y se reintenta 
    *
    */
      Serial.println("Intentando reconexion");
      habReconexion = false;
      warmUpSuave();

      byte resultado = initMQTT(0);
      byte intRes = 0; //intentos restantes para ejecutar la misma subrutina

      while((resultado < 8) && (intRes != INTENTOS)){
        delay(30000); //esperamos 1 minuto a reintentar
        intRes++;
        resultado = initMQTT(resultado);
      }

      //al salir si intRes es igual a INTENTOS se reinicia el ESP32
      if(intRes == INTENTOS){
        MQTTconectado = false;
      }else{
        intRes = 0; //Si no reiniciamos variable
        MQTTconectado = true;
        
      }

      if(MQTTconectado){
        //Si el anterior paso se logro iniciamos suscripcion
        while((!subMQTT(MQTT_SUBTOPIC)) && (intRes != INTENTOS)){
          delay(30000); //esperamos 1 minuto a reintentar 
          intRes++;
        }

        //al salir si intRes es igual a INTENTOS se reinicia el ESP32
        if(intRes ==   INTENTOS){
          MQTTconectado = false;
        }else{
          MQTTconectado = true;
          Serial1.println("AT+CSQ");
          Serial1.find("+CSQ: ");
          String senial = Serial1.readStringUntil(',');
          pubMQTT("{\"estado\":\"Reconexion\",\"senial\":"+senial+"}");

          Serial1.readString(); //Limpieza del buffer
        }
      }else{
        //no se continua con el proceso y se indica
        MQTTconectado = false;

      }
  













  }






}



//Vamos a devolver true si es que el warmup del modem fue correcto
/* En esta implementacion del warmup se verifica que la configuración sea correcta 
* si no lo es se da la configuracion correcta 
*/  
bool mdmWarmUp(){

  //Agregamos un timeout de 30s, el modem toma alrededor de 20s una vez se ha energizado para empezar a
  //recibir comandos AT de forma efectiva 
  Serial1.setTimeout(30000);

  //Buscamos la cadena 'RDY' ya que es la respuesta que entrega el modem al reiniciarse
  if(!Serial1.find("RDY")){
    //Si caemos en este bloque significa que el mdoem ya se encontraba encendido
    
  }
  //1.- Reiniciar el modem a configuracion predeterminada
  Serial1.setTimeout(1000);
  Serial1.readString();
  Serial1.println("AT+CFUN=1,1");


  //Vamos a esperar a que el modem responda 
  while(Serial1.available() < 50){}


  Serial1.readString(); //<- ESTE NO, FUNCIONA PARA LIMPIAR EL BUFFER


  

  //2.- DESACTIVAR ECHO DE LOS COMANDOS
  /*Si no existe respuesta correcta se trata de nuevo el comando hasta lo que indique INTENTOS
  * Si en esos intentos no se pudo se sale 
  * Este se hace si o si en el warmup
  */
  modemComando("ATE0");

  Serial1.readString();//Limpiamos buffer



  //3.- HORA Y FECHA AUTOMATICAS 
  Serial1.println("AT+CTZU?"); //preguntamos que configuracion tiene el modem
  
  while(Serial1.available() < 7){}

  if(Serial1.find("+CTZU: 1")){
    //No se hace anda es la respuesta que se busca, se limpia buffer
    Serial.println("Hora y fecha automaticas ya estaba configurado");
    Serial1.readString();
  }else{
    //Se manda el comando para ser configurado
    if(!modemComando("AT+CTZU=1")){
      Serial.println("\nNo se pudo configurar fecha y hora automaticas");
    }
  }
  

  //4.- CHECAR MODEM CON CARRIER AUTOMATICO
  Serial1.println("AT+COPS?");
  while(Serial1.available() < 7){}
  if(Serial1.find("+COPS: 0")){
    //No se hace anda es la respuesta que se busca
    Serial.println("Carrier automatico ya estaba configurado");
    Serial1.readString();
  }else{
    //Se manda el comando para ser configurado
    if(!modemComando("AT+COPS=0")){
      Serial.println("\nNo se pudo configurar carrier automatico");
    }
  }
  

  //5.- CHECAR APN CORRECTO
  Serial1.println("AT+CGDCONT?");
  while(Serial1.available() < 7){}
  if(Serial1.find("+CGDCONT: 1,\"IP\",\"data.mono\"")){
    //No se hace anda es la respuesta que se busca
    Serial.println("APN ya estaba configurado");
    Serial1.readString();
  }else{
    //Se manda el comando para ser configurado
    if(!modemComando("AT+CGDCONT=1,\"IP\",\"data.mono\"")){
      Serial.println("\nNo se pudo configurar APN");
    }
  }


  //Al finalizar regresamos TRUE
  return true;

}

//Funcion para enviar comandos al modem con reintentos
bool modemComando(String AT){

  //Variable que cuenta cuantos intentos se han llevado a cabo en un comando AT
  byte mdmIntRestantes = 0;

  Serial1.println(AT);

  while(!Serial1.find("OK")){
    Serial1.println(AT);
   
    if(++mdmIntRestantes == INTENTOS){
      //Si se llega al maximo de intentos salimos del warmup
      return false;
    }
    //Nos esperamos un momento para realizar un nuevo intento
    delay(200);//CAMB se puede usar otra base de tiempo si esta no funciona
  }
  //Limpiamos el buffer Serial
  Serial1.readString();

  return true;
  

}

/*
* Se regresa un numero entero del 0 al 2 que denota en que paso fallo la inicializacion
* y se retomara a a partir de ahi en el siguiente intento
*/
byte initMQTT(byte paso){


  switch(paso){

    case 0:
      //Incializamos el MQTT en el modem
      Serial1.println("AT+CMQTTSTART");
      while(Serial1.available() < 11){}

      //Comprobacion del inicio del MQTT
      if(Serial1.find("+CMQTTSTART: 0")){
        Serial.println("\n0.- MQTT INICIADO");
      }else{
        Serial.println("\n0.- MQTT NO INICIADO");
        return 0;
      }
      Serial1.readString();// Limpiar el buffer
      //-------------------------------
      
    case 1:

      //Configuramos ID del MQTT
      Serial1.println("AT+CMQTTACCQ=0,\""+String(MQTT_ID)+"\"");//Configuramos ID para MQTT dado por Losant
      while(Serial1.available() < 2){}
      //Comprobacion de la configuracion del ID
      if(Serial1.find("OK")){
        Serial.println("\n1.- ID ASIGNADO");
      }else{
        Serial.println("\n1.- ID NO ASIGNADO");
        return 1;
      }
      Serial1.readString();// Limpiar el buffer
      //-------------------------------

    case 2:

      //Nos conectamos al broker de Losant
      Serial1.println("AT+CMQTTCONNECT=0,\"tcp://"+String(MQTT_HOST)+":"+String(MQTT_PUERTO)+"\",60,1,\""+String(MQTT_USER)+"\",\""+String(MQTT_PASS)+"\"");

      while(Serial1.available() < 13){}

      //Comprobacion de la conexion al broker
      if(Serial1.find("+CMQTTCONNECT: 0,0")){
        Serial.println("\n2.- CONECTADO A BROKER");
      }else{
        Serial.println("\n2.- NO CONECTADO A BROKER");
        return 2;
      }
      Serial1.readString();// Limpiar el buffer
      //-------------------------------

    default:
      break;
  }

  return 8; //Si todo sale bien devolvemos 8



}

//regresamos true si logramos suscribirnos a tema
bool subMQTT(String tema){

  //-------------------------------
  //Configuramos tema de suscripcion
  Serial1.println("AT+CMQTTSUBTOPIC=0,"+String(tema.length())+",1");
  delay(10);
  Serial1.print(tema);
  while(Serial1.available() < 2){}
  //Comprobacion de la suscripcion al tema
  if(Serial1.find("OK")){
    Serial.println("\n1.- Tema de suscripcion configurado");
  }else{
    Serial.println("\n1.- Tema de suscripcion NO configurado");
    //EXPLORAR LA POSIBILIDAD DE SALIR AQUI
    return false;
  }
  Serial1.readString();// Limpiar el buffer

  //-------------------------------
  //Configuramos tema de suscripcion parte 2
  Serial1.println("AT+CMQTTSUB=0");
  while(Serial1.available() < 8){}


  //Comprobacion del segundo paso
  if(Serial1.find("+CMQTTSUB: 0,0")){
    Serial.println("\n2.- Preparando tema de suscripcion");
  }else{
    Serial.println("\n2.- NO Preparando tema de suscripcion");
    return false;
  }
  Serial1.readString();// Limpiar el buffer

  //-------------------------------
  //Configuramos tema de suscripcion parte 3
  Serial1.println("AT+CMQTTSUB=0,"+String(tema.length())+",0");
  delay(10); //CAMBIAR base de tiempo
  Serial1.print(tema);
  while(Serial1.available() < 8){}

  //Comprobacion del segundo paso
  if(Serial1.find("+CMQTTSUB: 0,0")){
    Serial.println("\n3.- Suscrito a tema: "+tema);
  }else{
    Serial.println("\n3.- NO Suscrito a tema: "+tema);
    return false;
  } 
  Serial1.readString();// Limpiar el buffer

  //si todo sale bien regresamos true
  return true;

}

//funcion para solor econectarse a servicio MQTT
bool reconMQTT(){

  //Incializamos el MQTT en el modem
  Serial1.println("AT+CMQTTSTART");
  while(Serial1.available() < 11){}

  //Comprobacion del inicio del MQTT
  if(Serial1.find("+CMQTTSTART: 0")){
    Serial.println("\nMQTT REINICIADO");
  }else{
    Serial.println("\nMQTT NO REINICIADO");
    return false;
  }
  Serial1.readString();// Limpiar el buffer
  //-------------------------------

  //Configuramos ID del MQTT
  Serial1.println("AT+CMQTTACCQ=0,\""+String(MQTT_ID)+"\"");//Configuramos ID para MQTT dado por Losant
  while(Serial1.available() < 2){}
  //Comprobacion de la configuracion del ID
  if(Serial1.find("OK")){
    Serial.println("\n1.- ID ASIGNADO");
  }else{
    Serial.println("\n1.- ID NO ASIGNADO");
    return false;
  }
  Serial1.readString();// Limpiar el buffer
  //-------------------------------

  //Nos conectamos al broker de Losant
  Serial1.println("AT+CMQTTCONNECT=0,\"tcp://"+String(MQTT_HOST)+":"+String(MQTT_PUERTO)+"\",60,1,\""+String(MQTT_USER)+"\",\""+String(MQTT_PASS)+"\"");

  while(Serial1.available() < 13){}

  //Comprobacion de la conexion al broker
  if(Serial1.find("+CMQTTCONNECT: 0,0")){
    Serial.println("\n2.- RECONECTADO A BROKER");
  }else{
    Serial.println("\n2.- NO RECONECTADO A BROKER");
    return false;
  }
  Serial1.readString();// Limpiar el buffer
  //-------------------------------

  //Si todo sale bien regresamos true
  return true;

}

//Subrutina para desonectarse del servicio MQTT
void disMQTT(){
  Serial1.setTimeout(1000);

  //1.- Mandamos mensaje de desconexion
  Serial1.println("AT+CMQTTDISC=0,120");
  delay(5000); //CAMBIAR base de tiempo, delay de cinco segundos
  //Comprobacion de la desconexion al broker
  if(Serial1.find("+CMQTTDISC: 0,0")){
    Serial.println("\nDESCONECTADO A BROKER");
  }else{

  }
  //-------------------------------
  //2.- 'Soltamos' (release) el cliente
  Serial1.println("AT+CMQTTREL=0");

  //Comprobacion de la desconexion al broker
  if(Serial1.find("OK")){
    Serial.println("\nCLIENTE LIBERADO");
  }else{

  }

  //-------------------------------
  //3.- 'Soltamos' (release) el cliente
  Serial1.println("AT+CMQTTSTOP");

  String cepillo = Serial1.readString();

  /*/Comprobacion de la desconexion al broker
  if(Serial1.find("OK")){
    Serial.println("\nMQTT DETENIDO");
  }else{

  }*/

  Serial.println(cepillo);


}


void ISRbtn(){
  
  /*
  * Sirve para desconectarse de manera correcta del Broker
  *
  * Implementado por interrupcion de boton de la board (ESP32THING)
  */

  //desMQTT = true;

}

void ISR_reconexion(){

  habReconexion = true; //permitimos intento de reconexion

  habPING = true; //Habilitamos envio de PING

}

void timersInit(){

  tReinicio = timerBegin(0, 80, true);//HW timer 0, divisor de 80, cuenta arriba
  tMonitoreo = timerBegin(1, 80, true); //HW timer 1, divisor de 80, cuenta arriba

  timerAttachInterrupt(tReinicio, &ISR_Reinicio, true);//configuramos INT del reinicio periodico
  timerAttachInterrupt(tMonitoreo, &ISR_reconexion, true);//configuramos INT del reinicio periodico

  //Timer de alarma
  timerAlarmWrite(tReinicio, 3600000000, true); // configuramos el tiempo para reinicio (1 hr)
  timerAlarmEnable(tReinicio); //Habilitamos timer 

  //Timer de habilitacion de reintento de reconexion (y ping)
  timerAlarmWrite(tMonitoreo, 600000000, true); // configuramos el tiempo para reinicio (10 min)
  timerAlarmEnable(tMonitoreo); //Habilitamos timer 

  
  Serial.println("TIMERS CONFIGURADOS Y CORRIENDO");

} 


bool warmUpSuave(){


  Serial1.readString();// limpiamos cualquier cosa que este en buffer para evitar fallos
  Serial1.println("AT+CFUN=1,1");


  //Vamos a esperar a que el modem responda 
  while(Serial1.available() < 50){}


  Serial1.readString(); //<- ESTE NO, FUNCIONA PARA LIMPIAR EL BUFFER


  

  //2.- DESACTIVAR ECHO DE LOS COMANDOS
  /*Si no existe respuesta correcta se trata de nuevo el comando hasta lo que indique INTENTOS
  * Si en esos intentos no se pudo se sale 
  * Este se hace si o si en el warmup
  */
  modemComando("ATE0");

  Serial1.readString();//Limpiamos buffer



  //3.- HORA Y FECHA AUTOMATICAS 
  Serial1.println("AT+CTZU?"); //preguntamos que configuracion tiene el modem
  
  while(Serial1.available() < 7){}

  if(Serial1.find("+CTZU: 1")){
    //No se hace anda es la respuesta que se busca, se limpia buffer
    Serial.println("Hora y fecha automaticas ya estaba configurado");
    Serial1.readString();
  }else{
    //Se manda el comando para ser configurado
    if(!modemComando("AT+CTZU=1")){
      Serial.println("\nNo se pudo configurar fecha y hora automaticas");
    }
  }
  

  //4.- CHECAR MODEM CON CARRIER AUTOMATICO
  Serial1.println("AT+COPS?");
  while(Serial1.available() < 7){}
  if(Serial1.find("+COPS: 0")){
    //No se hace anda es la respuesta que se busca
    Serial.println("Carrier automatico ya estaba configurado");
    Serial1.readString();
  }else{
    //Se manda el comando para ser configurado
    if(!modemComando("AT+COPS=0")){
      Serial.println("\nNo se pudo configurar carrier automatico");
    }
  }
  

  //5.- CHECAR APN CORRECTO
  Serial1.println("AT+CGDCONT?");
  while(Serial1.available() < 7){}
  if(Serial1.find("+CGDCONT: 1,\"IP\",\"data.mono\"")){
    //No se hace anda es la respuesta que se busca
    Serial.println("APN ya estaba configurado");
    Serial1.readString();
  }else{
    //Se manda el comando para ser configurado
    if(!modemComando("AT+CGDCONT=1,\"IP\",\"data.mono\"")){
      Serial.println("\nNo se pudo configurar APN");
    }
  }


  //Al finalizar regresamos TRUE
  return true;

}

void ISR_Reinicio(){
  //subrutina para reinicio periodico de WattWatcher

  //cada que se llame esta subrutina habilitaremos el reinicio del wattwathcer
  horasReinicio++;

  if(horasReinicio == hrspReinicio){

    horasReinicio = 0;
    reinicioInt = true;

  }
  
  

}

//para publicar un mensaje a tema configurado en el inciio del codigo
void pubMQTT(String mensaje){

  //-------------------------------
  //Configuramos tema de publicacion
  Serial1.println("AT+CMQTTTOPIC=0,"+String(String(MQTT_DEVICEPUB).length()));
  delay(10);
  Serial1.print(MQTT_DEVICEPUB);
  while(Serial1.available() < 2){}
  //Comprobacion de la suscripcion al tema
  if(Serial1.find("OK")){
    Serial.println("\n1.- Tema de publicacion configurado");
  }else{
    Serial.println("\n1.- Tema de publicacion NO configurado");
    //si es que se falla en este paso lo mas probable es que estemos desconectados
    MQTTconectado = false;
    //EXPLORAR LA POSIBILIDAD DE SALIR AQUI
    Serial1.readString();// Limpiar el buffer
    return;
  }
  Serial1.readString();// Limpiar el buffer

  //-------------------------------
  //Configuramos payload
  Serial1.println("AT+CMQTTPAYLOAD=0,"+String(mensaje.length()));
  delay(10);
  Serial1.print(mensaje);
  while(Serial1.available() < 2){}
  //Comprobacion del payload
  if(Serial1.find("OK")){
    Serial.println("\n2.- Payload configurado");
  }else{
    Serial.println("\n2.- Payload NO configurado");
    //si es que se falla en este paso lo mas probable es que estemos desconectados
    MQTTconectado = false;
    Serial1.readString();// Limpiar el buffer
    //EXPLORAR LA POSIBILIDAD DE SALIR AQUI
    return;
  }
  Serial1.readString();// Limpiar el buffer

  //-------------------------------
  //Publicamos cosa
  Serial1.println("AT+CMQTTPUB=0,1,60");
  while(Serial1.available() < 13){}

  //Comprobacion del payload
  if(Serial1.find("+CMQTTPUB: 0,0")){
    Serial.println("\n3.- Mensaje enviado");
  }else{
    Serial.println("\n3.- Mensaje NO enviado");
    //si es que se falla en este paso lo mas probable es que estemos desconectados
    MQTTconectado = false;
    Serial1.readString();// Limpiar el buffer
    //EXPLORAR LA POSIBILIDAD DE SALIR AQUI
    return;
  }
  Serial1.readString();// Limpiar el buffer

  //si todo sale bien regresamos true
  

}


























