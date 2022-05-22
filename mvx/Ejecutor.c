#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const char* twoOp[] = {
    "mov","add","sub",
    "swap","mul","div","cmp",
    "shl","shr","and","or","xor"};

const char* oneOp[] = {
    "sys","jmp","jz","jp","jn",
    "jnz","jnp","jnn","ldl",
    "ldh","rnd","not"};

const char* noOp[] = {"stop"};

const char* nombreReg[] = {
      "DS","SS","ES","CS","HP",
      "IP","SP","BP","CC","AC",
      "A","B","C","D","E","F"};

typedef unsigned int u32;

typedef struct Mv{

  int reg[16];

  int mem[8192];

}Mv;

//! DECLARACION DE PROTOTIPOS

u32 bStringtoInt(char* string);
void cargaMemoria(Mv* mv, char *argv[]);
void leeHeader(FILE* arch, char* rtadoHeader, Mv* mv);
void muestraCS(Mv mv);
void DecodificarInstruccion(int instr, int* numOp, int* codInstr,char* mnem);
void Ejecutar(Mv* mv,int codInstr, int numOp,int tOpA,int tOpB,int vOpA,int vOpB,char mnem[],char* argv[],int argc);
void AlmacenaEnRegistro(Mv* mv, int vOp, int dato, int numOp);
int ObtenerValorDeRegistro(Mv mv, int vOp, int numOp);
void step(Mv* mv,char* argv[],int argc);
void DecodificarOperacion(int instr,int *tOpA,int *tOpB,int *vOpA,int *vOpB, int numOp );
void obtenerOperando(int tOp,int vOp,char res[], int numOp);
void disassembler(Mv* mv,int tOpA, int tOpB, int vOpA, int vOpB, char* opA, char* opB,int numOp);
void modCC( Mv* mv, int op );
void mov(Mv* mv,int tOpA,int tOpB,int vOpA,int vOpB);
void add(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB);
void mul(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB);
void sub(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB);
void divi(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB);
void swap( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB );
void cmp( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB );
void and( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB );
void or( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB );
void xor( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB );
void shl( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB );
void shr( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB );
void sysWrite( Mv* mv );
void sysRead( Mv* mv );
void sysBreakpoint( Mv* mv, char* argv[],int argc,char mnem[], int llamaP);
void sysC(char* argv[], int argc);
void sys( Mv* mv, int tOpA, int vOpA ,char mnem[],char* argv[],int argc);
void falsoStep(Mv* mv,char opA[],char opB[],int i);
int apareceFlag(char* argv[],int argc, char* flag );
void jmp( Mv* mv,int tOpA,int vOpA );
void rnd(Mv *mv, int tOpA, int vOpA);
void slen(Mv *mv,int tOpA,int tOpB,int vOpA,int vOpB);
void smov(Mv *mv,int tOpA,int tOpB,int vOpA,int vOpB);
void scmp(Mv *mv,int tOpA,int tOpB,int vOpA,int vOpB);
void push(Mv *mv,int tOpA,int vOpA);
void pop(Mv *mv,int tOpA,int vOpA);
void call(Mv *mv,int tOpA,int vOpA);
void ret();
int calculaDireccion(Mv mv, int vOp);
int calculaIndireccion(Mv mv, int vOp);

int main(int argc, char *argv[]) {

  Mv mv;

  cargaMemoria(&mv, argv);

  printf("Codigo: \n");
  do{
    step(&mv,argv,argc);
  }while( (0 <= (mv.reg[5] & 0x0000FFFF) ) && ( (mv.reg[5] & 0x0000FFFF) < (mv.reg[0] & 0x0000FFFF) ));

  return 0;

}

void cargaMemoria(Mv* mv, char *argv[]){

  char linea[32];
  char rtadoHeader[] = "";
  int direccion;

  FILE* arch = fopen(argv[1],"r");

  if( arch ){
    leeHeader(arch, rtadoHeader, mv);
    if( strcmp(rtadoHeader,"") == 0 ){
      while( fgets(linea, 32, arch) ){
        direccion = calculaDireccion(*mv, mv->reg[0]);
        mv->mem[direccion] = bStringtoInt(linea);
        mv->reg[0]++;
      }
    }else
      printf("%s", rtadoHeader);
  }else
    printf("[!] Error al tratar de abrir el archivo. ");

  fclose(arch);

}


void cargaRegistros(Mv* mv, int* bloquesHeader){

  //Inicializacion de CS
  mv->reg[3] = (bloquesHeader[4] << 16) & 0xFFFF0000; //(H) y (L)

  //DS a continuacion del CS
  mv->reg[0] = (bloquesHeader[1] << 16) & 0xFFFF0000; //(H)
  mv->reg[0] +=  bloquesHeader[4];                    //(L)

  //ES a continuacion del DS
  mv->reg[2] = (bloquesHeader[3] << 16) & 0xFFFF0000; //(H)
  mv->reg[2] +=  bloquesHeader[1];                    //(L)

  //SS a continuacion del ES
  mv->reg[1] = (bloquesHeader[2] << 16) & 0xFFFF0000; //(H)
  mv->reg[1] +=  bloquesHeader[3];                   //(L)

  //Inicializacion de HP
  mv->reg[4] = 0x00020000;

  //Inicializacion de IP
  mv->reg[5] = 0x00030000;

  //Inicializacion de SP
  mv->reg[6] = 0x00010000;
  mv->reg[6] += (mv->reg[1] >> 16);

  //Inicializacion de BP
  mv->reg[7] = mv->reg[7] & 0x00010000;


}

//Lee los bloques del header y verifica si el archivo puede ser leido
void leeHeader(FILE* arch, char* rtadoHeader, Mv* mv){

  char linea[32]; //32 bits
  int bloquesHeader[6];
  int i;
  char bloque0[5];
  char bloque5[5];
  int suma;

  //Lectura de los 6 bloques
  for( i = 0 ; i < 6 ; i++){
      fgets(linea, 32, arch);
      bloquesHeader[i] = bStringtoInt(linea);
  }

  //Cargo el contenido del primer bloque
  bloque0[0] = (char) (bloquesHeader[0] & 0xFF000000) >> 24; //Me quedo con el primer caracter
  bloque0[1] = (char) (bloquesHeader[0] & 0x00FF0000) >> 16;
  bloque0[2] = (char) (bloquesHeader[0] & 0x0000FF00) >> 8;
  bloque0[3] = (char) (bloquesHeader[0] & 0x000000FF);
  bloque0[4] = '\0';

  //Cargo el contenido del quinto bloque
  bloque5[0] = (char) (bloquesHeader[5] & 0xFF000000) >> 24; //Me quedo con el primer caracter
  bloque5[1] = (char) (bloquesHeader[5] & 0x00FF0000) >> 16;
  bloque5[2] = (char) (bloquesHeader[5] & 0x0000FF00) >> 8;
  bloque5[3] = (char) (bloquesHeader[5] & 0x000000FF);
  bloque5[4] = '\0';


  if( (strcmp(bloque0,"MV-2") == 0) && (strcmp(bloque5,"V.22") == 0) ){
    suma = 0;
    for( i = 1; i < 5 ; i++){
      suma += bloquesHeader[i];
    }
    if( suma > 8192 )
      strcpy(rtadoHeader,"[!] El proceso no puede ser cargado por memoria insuficiente");
    else
      cargaRegistros(mv, bloquesHeader);
  }else
    strcpy(rtadoHeader,"[!] El formato del archivo .mv2 no es correcto");
}


//Muestra todas las instrucciones cargadas en memoria
void muestraCS(Mv mv){

  int i;
  int direccion = calculaDireccion(mv, mv.reg[0]);
  int tamSegm = mv.reg[0] >> 16;
  for( i = 0; i < tamSegm; i++){
    printf("%08X\n",mv.mem[direccion++]);
  }

}

//Verifica si un numero es negativo, y en caso de serlo, agrega las F faltantes a su izquierda
int complemento2(int vOp, int numOp ){

  int c2 = vOp;

  if( numOp == 2 && c2 < 0 ){
    if( (vOp & 0x800) == 0x800 ) // Caso 12 bits
      c2 = vOp | 0xFFFFF000;
    else
      if( (vOp & 0x80) == 0x80 ) //Caso 8 bits
        c2 = vOp | 0xFFFFFF00;
  }
  else if( numOp == 1 && c2 < 0 )
    if( (vOp & 0x8000) == 0x8000 ){ //caso 16 bits
      c2 = vOp | 0xFFFF0000;
    }else
      if( (vOp & 0x80) == 0x80 ) //Caso 8 bits
        c2 = vOp | 0xFFFFFF00;

  return c2;

}

//Decodifica segun la cantidad de operandos de la funcion, identificando:
//el codigo de instruccion, el numero de operandos, y el correspondiente mnemonico
void DecodificarInstruccion(int instr, int* numOp, int* codInstr,char* mnem){

  if( (instr & 0xFF000000) >> 24 == 0xFF ){ //Instruccion sin operandos
    *numOp = 0;
    *codInstr = (instr & 0x00F00000) >> 20;
    strcpy(mnem,noOp[0]);
  }else
  if( (instr & 0xF0000000) >> 28 == 0xF ){ //Instruccion con 1 operando
    *numOp = 1;
    *codInstr = (instr & 0x0F000000) >> 24; //
    strcpy(mnem,oneOp[*codInstr]);
  }else{ //Instruccion con 2 operando
    *numOp = 2;
    *codInstr = (instr & 0xF0000000) >> 28;
    strcpy(mnem,twoOp[*codInstr]);
  }

}

//En base al numero de operandos y al codigo de instruccion,
//ejecuta la instruccion segun su correspondiente codigo
void Ejecutar(Mv* mv,int codInstr, int numOp,int tOpA,int tOpB,int vOpA,int vOpB,char mnem[],char* argv[],int argc){

  if( numOp == 2 ){
    switch( codInstr ){
      case 0x0: //!mov
        mov(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x1: //!add
        add(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x2: //!sub
        sub(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x3: //!swap
        swap(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x4: //!mul
        mul(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x5: //!div
        divi(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x6: //!cmp
        cmp(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x7: //!shl
        shl(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x8: //!shr
        shr(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0x9: //!and
        and(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0xA: //!or
        or(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0xB: //!xor
        xor(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0xC: //!slen
        slen(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0xD: //!smov
        smov(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      case 0xE: //!scmp
        scmp(mv, tOpA, tOpB, vOpA, vOpB);
        break;
      }
  }else
  if( numOp == 1 )
    switch( codInstr ){
      case 0x0: //sys
          sys(mv,tOpA,vOpA, mnem,argv,argc);
        break;
      case 0x1: //!jmp
          jmp(mv,tOpA,vOpA);
        break;
      case 0x2: //!jz
          if((mv->reg[8] & 0x00000001)==1)
            jmp(mv,tOpA,vOpA);
        break;
      case 0x3: //!jp
          if((mv->reg[8] & 0x80000000)>>31==0)
            jmp(mv,tOpA,vOpA);
        break;
      case 0x4: //!jn
          if((mv->reg[8] & 0x80000000)>>31==1)
            jmp(mv,tOpA,vOpA);
        break;
      case 0x5: //!jnz
          if((mv->reg[8] & 0x00000001) == 0)
            jmp(mv,tOpA,vOpA);
        break;
      case 0x6: //!jnp
          if((mv->reg[8] & 0x80000000)>>31 == 1 || (mv->reg[8] & 0x00000001) == 1)
            jmp(mv,tOpA,vOpA);
        break;
      case 0x7: //!jnn
          if((mv->reg[8] & 0x80000000) >> 31 == 0 || (mv->reg[8] & 0x00000001) == 1)
            jmp(mv,tOpA,vOpA);
        break;
      case 0x8: //!ldl
        vOpB = DevuelveValor(*mv, tOpA, vOpA, 1);
        vOpB = (vOpB & 0x0000FFFF);
        mv->reg[9] = (mv->reg[9] & 0xFFFF0000) | vOpB;
        break;
      case 0x9: //!ldh
        vOpB = DevuelveValor(*mv,tOpA,vOpA, 1);
        vOpB = (vOpB & 0x0000FFFF);
        mv->reg[9] = ( mv->reg[9] & 0x0000FFFF ) | ( vOpB << 16 ) ;
        break;
      case 0xA: //!rnd
        rnd(mv, tOpA, vOpA);
        break;
      case 0xB: //!not
        not( mv, vOpA, tOpA );
        break;
      case 0xC: //!push
        push(mv, tOpA, vOpA);
        break;
      case 0xD: //!pop
        pop(mv, tOpA, vOpA);
        break;
      case 0xE: //!call
        call(mv, tOpA, vOpA);
        break;
    }else
      switch(codInstr){
        case 0x0: //!ret
          ret();
          break;
        case 0x1: //!stop
          stop();
          break;
      }

}

void pop(Mv *mv, int tOpA, int vOpA){

    //Recupero el tamano del SS
    int tamSS = (mv->reg[1] & 0xFFFF0000) >> 16;

    //Recupero el inicio del SS
    int inicioSS = (mv->reg[1] & 0x0000FFFF);

    //Recupero el valor de SP
    int valorSP = (mv->reg[6] & 0x0000FFFF);

    if( valorSP > (inicioSS + tamSS) ){
      printf (" STACK UNDERFLOW ");
      exit(-1);
    }
    else{
      //tOpB=2 porque seria directo y vOpB = mv->reg[6] porque paso donde apunta la pila
      mov(mv,tOpA,2,vOpA,mv->reg[6]);
      mv->reg[6]++;
    }
}

void push(Mv *mv, int tOpA, int vOpA){

    int direccion;

    //Recupero el inicio del SS
    int inicioSS = mv->reg[1] & 0x0000FFFF;

    //Recupero el valor de SP
    int valorSP = mv->reg[6] & 0x0000FFFF;

    //Si el SP supera el SS, stack overflow
    if( mv->reg[6] <= inicioSS ){
      printf (" STACK OVERFLOW ");
      exit(-1);
    }
    else{
        mv->reg[6]--;       //Decremento SP
        valorSP--;
        if( tOpA == 0 )     //Inmediato
          mv->mem[valorSP] = vOpA;
        else
          if( tOpA == 1)    //De registro
            mv->mem[valorSP] = ObtenerValorDeRegistro(*mv, vOpA, 1);
          else              //Directo
            if( tOpA == 2 ){
              direccion = calculaDireccion(*mv, vOpA);
              mv->mem[valorSP] = mv->mem[ direccion ];
            }
            else{           //Indirecto
              direccion = calculaIndireccion(*mv, vOpA);
              mv->mem[valorSP] = mv->mem[direccion];
            }

    }
}

void ret(Mv *mv){

    //Recupero el tamano del SS
    int tamSS = (mv->reg[1] & 0xFFFF0000) >> 16;

    //Recupero el inicio del SS
    int inicioSS = (mv->reg[1] & 0x0000FFFF);

    //Recupero el valor de SP
    int valorSP = (mv->reg[6] & 0x0000FFFF);

    if( valorSP > (inicioSS + tamSS) ){
      printf (" STACK UNDERFLOW ");
      exit(-1);
    }
    else{
      //"salta" colocando la direccion de retorno en el reg IP
      mv->reg[5] = mv->mem[valorSP++];
    }

}

void call(Mv *mv, int tOpA, int vOpA){

    int direccion;

    //Recupero el inicio del SS
    int inicioSS = mv->reg[1] & 0x0000FFFF;

    //Recupero el valor de SP
    int valorSP = mv->reg[6] & 0x0000FFFF;

    //Si el SP supera el SS, stack overflow
    if( mv->reg[6] <= inicioSS ){
      printf (" STACK OVERFLOW ");
      exit(-1);
    }
    else{
        mv->reg[6]--;
        valorSP--;
        mv->mem[valorSP] = mv->reg[5]++; //Guardo el IP para luego retornar
        if( tOpA == 0 ){             //Inmediato
            mv->reg[5] = vOpA;
        }else
            if( tOpA == 1 ){         //De registro
                mv->reg[5] = ObtenerValorDeRegistro(*mv,vOpA, 1);
            }else if( tOpA == 2 ){   //Directo
              direccion = calculaDireccion(*mv, vOpA);
              mv->reg[5] = mv->mem[direccion];
            }else{                   //Indirecto
              direccion = calculaIndireccion(*mv, vOpA);
              mv->reg[5] = mv->mem[direccion];
            }
    }
}

void stop(Mv* mv){

  mv->reg[5] = mv->reg[0] & 0x0000FFFF;

}

void not( Mv* mv, int vOp, int tOp ){

  int direccion;
  int valorNeg = ~DevuelveValor(*mv, tOp, vOp);

  if( tOp == 1 ){ //De registro
    AlmacenaEnRegistro( mv, vOp, valorNeg, 1 );
  }else if( tOp == 2 ){ //Directo
    direccion = calculaDireccion(*mv, vOp);
    mv->mem[direccion] = valorNeg;
  }
  modCC(mv,valorNeg);


}

//Almacena en un sector de registro un determinado "dato", valor
void AlmacenaEnRegistro(Mv* mv, int vOp, int dato, int numOp){

  int sectorReg = (vOp & 0x30) >> 4;
  int idReg = (vOp & 0xF);

  /*
  Todo dato que se almacena en un registro, debe pasar por el if del complemento a 2
  Si es positivo, no se modifica, queda intacto
  */
  dato = complemento2(dato, numOp);

  switch( sectorReg ){
    case 0: //4 bytes
      mv->reg[idReg] = dato;
      break;
    case 1: //4to byte
      mv->reg[idReg] = (mv->reg[idReg] & 0xFFFFFF00) | (dato & 0x000000FF);
      break;
    case 2: //3er byte
      mv->reg[idReg] = (mv->reg[idReg] & 0xFFFF00FF) | ((dato & 0x000000FF) << 8);
      break;
    case 3: //3er y 4to byte
      mv->reg[idReg] = (mv->reg[idReg] & 0xFFFF0000) | (dato & 0x0000FFFF);
      break;
  }
}

//Obtengo un valor entero de un determinado sector de registro
int ObtenerValorDeRegistro(Mv mv, int vOp, int numOp){

  int sectorReg = (vOp & 0x30) >> 4;
  int idReg = (vOp & 0xF);

  int valor;

  switch( sectorReg ){
    case 0:
      valor = mv.reg[idReg];
      break;
    case 1:
      valor = mv.reg[idReg] & 0x000000FF;
      break;
    case 2:
      valor = (mv.reg[idReg] & 0x0000FF00) >> 8;
      break;
    case 3:
      valor = mv.reg[idReg] & 0x0000FFFF;
      break;
  }

/*
Devuelve el valor ya complementado si resulta negativa
Cambia los ceros de la izquierda por F's,
ya que el VALOR(variable) esta complementado, pero con ceros a la izquierda, no cumpliendo el complemento a 2 "total"
*/
  return complemento2(valor,numOp);
}

void muestraInstruccion( Mv mv, int instr, int numOp, char* opA, char* opB, char* mnem ){

  if( numOp == 2 ){
    printf("[%04d]:  %08X   %d: %s   %s,%s\n",mv.reg[5]-1,instr,mv.reg[5],mnem,opA,opB);
  }else if( numOp == 1 ){
    printf("[%04d]:  %08X   %d: %s   %s\n",mv.reg[5]-1,instr,mv.reg[5],mnem,opA);
  }else{
    printf("[%04d]:  %08X   %d: %s  \n",mv.reg[5]-1,instr,mv.reg[5],mnem);
  }

}

void step(Mv* mv,char* argv[],int argc){

  int instr;
  int numOp, codInstr;
  char mnem[5], opA[7] = "",opB[7] = "";
  int tOpA, tOpB;
  int vOpA, vOpB;

  instr = mv->mem[mv->reg[5]]; //fetch
  DecodificarInstruccion(instr, &numOp, &codInstr, mnem); //Decodifica el numero de operandos de la instruccion
  mv->reg[5]++;
  DecodificarOperacion(instr, &tOpA, &tOpB, &vOpA, &vOpB, numOp);
  disassembler(mv, tOpA, tOpB, vOpA, vOpB, opA, opB, numOp);
  muestraInstruccion(*mv, instr, numOp, opA, opB, mnem);
  Ejecutar(mv, codInstr, numOp, tOpA, tOpB, vOpA, vOpB, mnem, argv, argc);
}

//Obtenemos los operandos que utilizaremos en el disassembler
void obtenerOperando(int tOp, int vOp, char res[], int numOp){

    int aux=0;
    char auxS[7] = "";

    vOp = complemento2(vOp,numOp);

    if( tOp == 0 ){ //Inmediato
      itoa(vOp, res, 10);
    }else if( tOp == 1 ){ // De registro
      aux = (vOp & 0x30) >> 4; //Obtenemos el sector de registro
      strcpy(auxS, nombreReg[vOp & 0xF] );//obtengo nombre de reg (a,b,c,etc)
      if((vOp & 0xF)<10){
        strcpy(res,auxS);
      }
      else{ //Estamos en los registros de prop. gral (A,B,...,F)
          if( aux == 0 ){ //4 bytes
            res[0]='E';
            res[1]=auxS[0];
            res[2]='X';
         }
         else
           if(aux==1){//4to byte
             res[0]=auxS[0];
             res[1]='L';
           }
           else
            if(aux==2){//3er byte
              res[0]=auxS[0];
              res[1]='H';
            }
            else{//2 bytes
               res[0]=auxS[0];
               res[1]='X';
            }
      }
    }
    else if( tOp == 2 ){//si es directo
        res[0]='[';
        itoa(vOp,auxS,10);
        strcat(res,auxS);
        strcat(res,"]");
    }
}

void disassembler(Mv* mv,int tOpA, int tOpB, int vOpA, int vOpB, char* opA, char* opB,int numOp){ //Actualizar numOp

  char resA[7]={""};
  char resB[7]={""};

  if( numOp == 1 ){
    obtenerOperando(tOpA,vOpA,resA, numOp);
    strcpy(opA,resA);
  }else
    if( numOp == 2 ){//con dos operandos
      obtenerOperando(tOpA,vOpA,resA,numOp);
      strcpy(opA,resA);
      obtenerOperando(tOpB,vOpB,resB,numOp);
      strcpy(opB,resB);
    }
 }

//Obtenemos segun el numero de operando:
//tipo de operandos, valor de operandos
void DecodificarOperacion(int instr,int *tOpA,int *tOpB,int *vOpA,int *vOpB, int numOp ){

  switch ( numOp ){
    case 2:
      *tOpA = (instr & 0x0C000000) >> 26;
      *tOpB = (instr & 0x03000000) >> 24;
      *vOpA = (instr & 0x00FFF000) >> 12 ;
      *vOpB = (instr & 0x00000FFF);
      if( *tOpA == 0 )
        *vOpA = complemento2(*vOpA,2);
      if( *tOpB == 0 )
        *vOpB = complemento2(*vOpB,2);
      break;
    case 1:
      *tOpA = (instr & 0x00C00000) >> 22;
      *vOpA = (instr & 0x0000FFFF);
      *vOpA = complemento2(*vOpA, 1); //ojito
      *tOpB = 0;
      *vOpB = 0;
      break;
    case 0:
      *tOpA = 0;
      *vOpA = 0;
      *tOpB = 0;
      *vOpB = 0;
      break;
    default:
      printf("[!] Numero de operacion no valido ");
      break;
  }

}

//Convierte las instrucciones "binarias" almacenada en formato string
//a unsigned int para luego almacenarlas en memoria
u32 bStringtoInt(char* string){

  int i;
  u32 ac = 0;
  int exp = 0;
  int auxb = 0;
  for( i = 31; i >= 0 ; i-- ){
    if( string[i] == '1'){
       ac += pow(2,exp);
    }
    exp++;
  }
  return ac;
}

int calculaIndireccion(Mv mv, int vOp){

  int codReg = vOp & 0x000F;
  int offset = vOp >> 4;
  int regH = mv.reg[codReg] >> 16;
  int regL = mv.reg[codReg] & 0x0000FFFF;
  int segm;
  int tamSegm;
  int inicioSegm;

  /*[eax]
  eax = 0003XXXX
  eax = 0002XXXX
  eax = 0001XXXX
  eax = 0000XXXX
  */

  //Recupero la info del segmento

  segm = mv.reg[regH]; //CS, DS, SS, ES

  //Obtengo tamano del segmento
  tamSegm = segm >> 16;

  //Obtengo direccion relativa al segmento
  inicioSegm = segm & 0x0000FFFF;

  if( (mv.reg[codReg] < 0) && ((regL + offset) > tamSegm) && ((regL + offset) < inicioSegm) ){
    printf("Segmentation fault");
    exit(-1);
  }
  else
    return inicioSegm + regL + offset;
    //Retorno la direccion relativa al segmento

}

int calculaDireccion(Mv mv, int vOp){

  int segm;
  int tamSegm;
  int inicioSegm;

  //Recupero la informacion del segmento al cual quiero direccionarme
  segm = mv.reg[vOp >> 16]; // 0000, 0001, 0002, 0003

  //Obtengo tamano del segmento
  tamSegm = segm >> 16;

  //Obtengo direccion relativa al segmento
  inicioSegm = segm & 0x0000FFFF;

  if( (vOp < 0) &&(vOp > tamSegm) ){
    printf("Segmentation fault");
    exit(-1);
  }
  else
    return inicioSegm + vOp; //Retorno la direccion relativa


}

void mov(Mv* mv,int tOpA,int tOpB,int vOpA,int vOpB){

  //Variables de op de reg.
  int codReg;
  int indireccion1, indireccion2;
  int direccion1, direccion2;
  int sectorReg;

  if( tOpA == 3 ){ //! OpA -> Indirecto
    indireccion1 = calculaIndireccion(*mv, vOpA);
      if( tOpB == 0 )//mov [eax + x] = 10
        mv->mem[indireccion1] = vOpB;
      else
        if( tOpB == 1 ) //mov [eax+x] = eax
          mv->mem[indireccion1] = ObtenerValorDeRegistro(*mv, vOpB, 2);
        else
          if( tOpB == 2 ){ //mov [eax+x] = [10]
            direccion2 = calculaDireccion(*mv, vOpB);
            mv->mem[indireccion1] = mv->mem[direccion2];
          }else
            if( tOpB == 3 ){ //mov [eax+x] = [ebx+2];
              indireccion2 = calculaIndireccion(*mv, vOpB);
              mv->mem[indireccion1] = mv->mem[indireccion2];
            }
  }else if( tOpA==2 ){ //!OpA -> directo -> [10]

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ) // mov [10] 10
      mv->mem[direccion1] = vOpB;
    else
    if( tOpB == 1 ) // mov [10] AX
      mv->mem[direccion1] = ObtenerValorDeRegistro(*mv,vOpB,2);
    else
      if( tOpB == 2 ){         // mov [vOpA] [vOpB]
        direccion2 = calculaDireccion(*mv, vOpB);
        mv->mem[direccion1] = mv->mem[direccion2];
      }else{
        indireccion2 = calculaIndireccion(*mv, vOpB);
        mv->mem[direccion1] = mv->mem[indireccion2];
      }

  }else if( tOpA == 1 ){          //!Op -> De registro -> EAX

      if( tOpB == 0 ){ // mov AX 10
        AlmacenaEnRegistro(mv, vOpA, vOpB, 2);
      }else
        if( tOpB == 2){ // mov AX [10]
          direccion2 = calculaDireccion(*mv, vOpB);
          AlmacenaEnRegistro( mv, vOpA, mv->mem[direccion2], 2 );
        }else
          if( tOpB == 1) //mov AX EAX
            AlmacenaEnRegistro( mv, vOpA, ObtenerValorDeRegistro(*mv,vOpB,2), 2 );
          else {
            indireccion2 = calculaIndireccion(*mv, vOpB);
            AlmacenaEnRegistro(mv, vOpA, mv->mem[indireccion2], 2);
          }

      }
}

void add(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB){

  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

  if( tOpA == 3 ){ //! OpA -> Indirecto
    indireccion1 = calculaIndireccion(*mv, vOpA);
      if( tOpB == 0 )//mov [eax + x] = 10
        mv->mem[indireccion1] += vOpB;
      else
        if( tOpB == 1 ) //mov [eax+x] = eax
          mv->mem[indireccion1] += ObtenerValorDeRegistro(*mv, vOpB, 2);
        else
          if( tOpB == 2 ){ //mov [eax+x] = [10]
            direccion2 = calculaDireccion(*mv, vOpB);
            mv->mem[indireccion1] += mv->mem[direccion2];
          }else
            if( tOpB == 3 ){ //mov [eax+x] = [ebx+2];
              indireccion2 = calculaIndireccion(*mv, vOpB);
              mv->mem[indireccion1] += mv->mem[indireccion2];
            }
  }
  else if( tOpA==2 ){ //! tOpA -> Directo [vOpA]

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){ // add [10] 10
      mv->mem[direccion1] += vOpB;
    }else
      if( tOpB == 2){ //add [vOpA] [vOpB]
        direccion2 = calculaDireccion(*mv, vOpB);
        mv->mem[direccion1] += mv->mem[direccion2];
      }else
        if( tOpB == 1 )//directo y reg
          mv->mem[direccion1] += ObtenerValorDeRegistro(*mv,vOpB,2);
        else{
          indireccion2 = calculaIndireccion(*mv, vOpB);
          mv->mem[direccion1] += mv->mem[indireccion2];
        }


    modCC(mv, mv->mem[mv->reg[0] + vOpA]);

  }else
    if( tOpA == 1 ){ //! tOpA -> DeRegistro EAX

      aux = ObtenerValorDeRegistro(*mv,vOpA,2);
      if( tOpB == 0 ){ // add reg 10
        aux += vOpB;
      }else if( tOpB == 2){ //add reg [vOpB]
        direccion2 = calculaDireccion(*mv, vOpB);
        aux += mv->mem[direccion2];
      }else if( tOpB == 1 ) //reg y reg
        aux += ObtenerValorDeRegistro(*mv,vOpB,2);
      else{
        indireccion2 = calculaIndireccion(*mv, vOpB);
        aux += mv->mem[indireccion2];
      }
      AlmacenaEnRegistro(mv, vOpA, aux, 2);
      modCC(mv, aux);
    }
}

void modCC( Mv* mv, int op ){

  mv->reg[8] = 0;
  if( op == 0 )      // = 0 -> Prende bit menos significativo
    mv->reg[8] = mv->reg[8] | 0x00000001;
  else if( op < 0 )  // < 0 -> Prende bit mas significativo
    mv->reg[8] = mv->reg[8] | 0x80000000;
}

void mul(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB){

  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;


  if( tOpA == 3 ){ //! OpA -> Indirecto
    indireccion1 = calculaIndireccion(*mv, vOpA);
      if( tOpB == 0 )//mov [eax + x] = 10
        mv->mem[indireccion1] *= vOpB;
      else
        if( tOpB == 1 ) //mov [eax+x] = eax
          mv->mem[indireccion1] *= ObtenerValorDeRegistro(*mv, vOpB, 2);
        else
          if( tOpB == 2 ){ //mov [eax+x] = [10]
            direccion2 = calculaDireccion(*mv, vOpB);
            mv->mem[indireccion1] *= mv->mem[direccion2];
          }else
            if( tOpB == 3 ){ //mov [eax+x] = [ebx+2];
              indireccion2 = calculaIndireccion(*mv, vOpB);
              mv->mem[indireccion1] *= mv->mem[indireccion2];
            }
  }
  else if( tOpA==2 ){ //! tOpA -> Directo [vOpA]

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){ // add [10] 10
      mv->mem[direccion1] *= vOpB;
    }
    else if( tOpB == 2){ //add [vOpA] [vOpB]
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[direccion1] *= mv->mem[direccion2];
    }
    else if( tOpB == 1 ){//directo y de reg
      mv->mem[direccion1] *= ObtenerValorDeRegistro(*mv,vOpB,2);
    }
    else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[direccion1] *= mv->mem[indireccion2];
    }
    modCC(mv, mv->mem[direccion1]);

  }else
    if( tOpA == 1 ){ //! tOpA -> DeRegistro EAX

      aux = ObtenerValorDeRegistro(*mv,vOpA,2);
      if( tOpB == 0 ){ // add reg 10
        aux *= vOpB;
      }else if( tOpB == 2){ //add reg [vOpB]
        direccion2 = calculaDireccion(*mv, vOpB);
        aux *= mv->mem[direccion2];
      }else if( tOpB == 1){// reg reg
        aux *= ObtenerValorDeRegistro(*mv,vOpB,2);
      }else{
        indireccion2 = calculaIndireccion(*mv, vOpB);
        aux *= mv->mem[indireccion2];
      }
      AlmacenaEnRegistro(mv, vOpA, aux, 2);
      modCC(mv, aux);
    }
}

void sub(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB){

  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

    if( tOpA == 3 ){ //! OpA -> Indirecto

    indireccion1 = calculaIndireccion(*mv, vOpA);
      if( tOpB == 0 )//mov [eax + x] = 10
        mv->mem[indireccion1] -= vOpB;
      else
        if( tOpB == 1 ) //mov [eax+x] = eax
          mv->mem[indireccion1] -= ObtenerValorDeRegistro(*mv, vOpB, 2);
        else
          if( tOpB == 2 ){ //mov [eax+x] = [10]
            direccion2 = calculaDireccion(*mv, vOpB);
            mv->mem[indireccion1] -= mv->mem[direccion2];
          }else
            if( tOpB == 3 ){ //mov [eax+x] = [ebx+2];
              indireccion2 = calculaIndireccion(*mv, vOpB);
              mv->mem[indireccion1] -= mv->mem[indireccion2];
            }

  }else if( tOpA==2 ){ //! tOpA -> Directo [vOpA]

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){ // add [10] 10
      mv->mem[direccion1] -= vOpB;
    }
    else if( tOpB == 2){ //add [vOpA] [vOpB]
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[direccion1] -= mv->mem[direccion2];
    }
    else if( tOpB == 1){
     mv->mem[direccion1] -= ObtenerValorDeRegistro(*mv,vOpB,2);
    }
    else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[direccion1] -= mv->mem[indireccion2];
    }
    modCC(mv, mv->mem[direccion1]);

  }else if( tOpA == 1 ){ //! tOpA -> DeRegistro EAX

      aux = ObtenerValorDeRegistro(*mv,vOpA,2);
      if( tOpB == 0 ){ // add [10] 10
        aux -= vOpB;
      }
      else if( tOpB == 2){ //add [vOpA] [vOpB]
        direccion2 = calculaDireccion(*mv, vOpB);
        aux -= mv->mem[direccion2];
      }
      else if( tOpB == 1 ){
        aux -= ObtenerValorDeRegistro(*mv,vOpB,2);
      }
      else{
        indireccion2 = calculaIndireccion(*mv, vOpB);
        aux -= mv->mem[indireccion2];
      }
      AlmacenaEnRegistro(mv, vOpA, aux, 2);
      modCC(mv, aux);
    }
}

void divi(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB){

  int aux;
  int auxB;
  int cero = 0;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

  if( tOpA == 3 ){ //! OpA -> Indirecto

    indireccion1 = calculaIndireccion(*mv, vOpA);
    if( tOpB == 0 ){ // add [10] 10
      if( vOpB ){
        mv->reg[9] = mv->mem[indireccion1] % vOpB;
        mv->mem[indireccion1] /= vOpB;
      }
      else
        printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }else if( tOpB == 2){ //add [vOpA] [vOpB]
      direccion2 = calculaDireccion(*mv, vOpB);
      if( mv->mem[direccion2] ){
        mv->reg[9] = mv->mem[indireccion1] % mv->mem[direccion2];
        mv->mem[indireccion1] /= mv->mem[direccion2];
      }
      else
        printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }else if( tOpB == 1 ){
       auxB = ObtenerValorDeRegistro(*mv,vOpB,2);
       if( auxB ){
         mv->reg[9] = mv->mem[indireccion1] % auxB;
         mv->mem[indireccion1] /= auxB;
       }
       else
         printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      if( mv->mem[indireccion2] ){
        mv->reg[9] = mv->mem[indireccion1] % mv->mem[indireccion2];
        mv->mem[indireccion1] /= mv->mem[indireccion2];
      }else
        printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }

    modCC(mv, mv->mem[mv->reg[0] + vOpA]);
  }
  else if( tOpA==2 ){ //! tOpA -> Directo [vOpA]

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){ // add [10] 10
      if( vOpB ){
        mv->reg[9] = mv->mem[direccion1] % vOpB;
        mv->mem[direccion1] /= vOpB;
      }
      else
        printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }else if( tOpB == 2){ //add [vOpA] [vOpB]
      direccion2 = calculaDireccion(*mv, vOpB);
      if( mv->mem[direccion2] ){
        mv->reg[9] = mv->mem[direccion1] % mv->mem[direccion2];
        mv->mem[direccion1] /= mv->mem[direccion2];
      }
      else
        printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }else if( tOpB == 1 ){
       auxB = ObtenerValorDeRegistro(*mv,vOpB,2);
       if( auxB ){
         mv->reg[9] = mv->mem[direccion1] % auxB;
         mv->mem[direccion1] /= auxB;
       }
       else
         printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      if( mv->mem[indireccion2] ){
        mv->reg[9] = mv->mem[direccion1] % mv->mem[indireccion2];
        mv->mem[direccion1] /= mv->mem[indireccion2];
      }else
        printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }
    modCC(mv, mv->mem[direccion1]);
  }

  else
    if( tOpA == 1 ){ //! tOpA -> DeRegistro EAX

      aux = ObtenerValorDeRegistro(*mv,vOpA,2);

      if( tOpB == 0 ){ // add [10] 10
        if( vOpB ){
          mv->reg[9] = aux % vOpB;
          aux /= vOpB;
        }
        else
          cero = 1;
      }else if( tOpB == 2){ //add [vOpA] [vOpB]
        direccion2 = calculaDireccion(*mv, vOpB);
        if( mv->mem[direccion1] ){
          mv->reg[9] = aux % mv->mem[direccion2];
          aux /= mv->mem[direccion2];
        }
        else
          cero = 1;
      }else if( tOpB == 1 ){
        auxB = ObtenerValorDeRegistro(*mv,vOpB,2);
        if( auxB ){
          mv->reg[9] = aux % auxB;
          aux /= auxB;
        }
        else
          cero = 1;
      }else{
        indireccion2 = calculaIndireccion(*mv, vOpB);
        if( mv->mem[indireccion2] ){
          mv->reg[9] = aux % mv->mem[indireccion2];
          aux /= mv->mem[indireccion2];
        }
        else
          cero = 1;
      }
      if( !cero ) {
        AlmacenaEnRegistro(mv, vOpA, aux, 2);
        modCC(mv, aux);
      }
    }
}

void swap( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;


  if( tOpA == 3 ){

    indireccion1 = calculaIndireccion(*mv, vOpA);
    aux = mv->mem[indireccion1];

    if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[indireccion1] = mv->mem[direccion2];
      mv->mem[direccion2] = aux;
    }else if( tOpB == 1 ){
      mv->mem[indireccion1] = ObtenerValorDeRegistro(*mv, vOpB, 2);
      AlmacenaEnRegistro(mv, vOpB, aux,2);
    }else if ( tOpB == 3 ){
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[indireccion1] = mv->mem[indireccion2];
      mv->mem[indireccion2] = aux;
    }

  }else if( tOpA==2 ){ //! Op directo

    direccion1 = calculaDireccion(*mv,vOpA);
    aux = mv->mem[direccion1];

    if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[direccion1] = mv->mem[direccion2];
      mv->mem[direccion2] = aux;
    }else if( tOpB == 1 ){
      mv->mem[direccion1] = ObtenerValorDeRegistro(*mv, vOpB, 2);
      AlmacenaEnRegistro(mv, vOpB, aux,2);
    }else if ( tOpB == 3 ){
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[direccion1] = mv->mem[indireccion2];
      mv->mem[indireccion2] = aux;
    }

  }else if( tOpA ==1 ){ //!De registro

    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      AlmacenaEnRegistro(mv,vOpA,mv->mem[direccion2],2);
      mv->mem[direccion2] = aux;
    }else if( tOpB == 1 ){
      AlmacenaEnRegistro(mv, vOpA, ObtenerValorDeRegistro(*mv, vOpB,2),2);
      AlmacenaEnRegistro(mv, vOpB, aux,2);
    }else if( tOpB == 3){
      indireccion2 = calculaIndireccion(*mv, vOpB);
      AlmacenaEnRegistro(mv, vOpA, mv->mem[indireccion2] ,2);
      mv->mem[indireccion2] = aux;
    }

  }

}

void cmp( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int sub;
  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

  if( tOpA == 0 ){                 //! Op inmediato

    if( tOpB == 0 ){               // cmp 10, 11
      sub = vOpA - vOpB;
    }else if( tOpB == 2 ){         // cmp 10, [11]
      direccion2 = calculaDireccion(*mv, vOpB);
      sub = vOpA - mv->mem[direccion2];
    }else if( tOpB == 1) {                         // cmp 10, EAX
      sub = vOpA - ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      sub = vOpA - mv->mem[indireccion2];
    }

  }else if( tOpA == 2 ){           //! Op directo

     direccion1 = calculaDireccion(*mv, vOpA);
     aux = mv->mem[direccion1];

     if( tOpB == 0 ){              // cmp [10], 11
      sub = aux - vOpB;
    }else if( tOpB == 2 ){         // cmp [10], [11]
      direccion2 = calculaDireccion(*mv, vOpB);
      sub = aux - mv->mem[direccion2];
    }else if( tOpB == 1){                         // cmp [10], EAX
      sub = aux - ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      sub = aux - mv->mem[indireccion2];
    }

  }else if( tOpA == 1 ){                           //! Op de registro

    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 0 ){               // cmp EAX, 11
      sub = aux - vOpB;
    }else if( tOpB == 2 ){         // cmp EAX, [11]
      direccion2 = calculaDireccion(*mv, vOpB);
      sub = aux - mv->mem[direccion2];
    }else if( tOpB == 1){                         // cmp EAX, AX
      sub = aux - ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      sub = aux - mv->mem[indireccion2];
    }
  }else{

    indireccion2 = calculaIndireccion(*mv, vOpA);
    aux = mv->mem[indireccion2];

    if( tOpB == 0 ){
      sub = aux - vOpB;
    }else if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      sub = aux - mv->mem[direccion2];
    }else if( tOpB == 1){
      sub = aux - ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      sub = aux - mv->mem[indireccion2];
    }
  }

  modCC(mv, sub);

}

void and( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int op;
  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

  if( tOpA == 3){

    indireccion1 = calculaIndireccion(*mv, vOpA);

    if( tOpB == 0 ){
      mv->mem[indireccion1] &= vOpB;
    }else if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[indireccion1] &= mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[indireccion1] &= ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[indireccion1] &= mv->mem[indireccion2];
    }
    op = mv->mem[indireccion1];

  }else if( tOpA==2 ){ //! Op directo

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){
      mv->mem[direccion1] &= vOpB;
    }else if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[direccion1] &= mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[direccion1] &= ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[direccion1] &= mv->mem[indireccion2];
    }

    op = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro
    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 0 ){
      op = aux & vOpB;
    }else if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      op = aux & mv->mem[direccion2];
    }else if( tOpB == 1 ){
      op = aux & ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      op = aux & mv->mem[indireccion2];
    }
    AlmacenaEnRegistro(mv, vOpA, op,2);

  }
  modCC(mv, op);

}

void or( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int op;
  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

  if( tOpA == 3){

    indireccion1 = calculaIndireccion(*mv, vOpA);

    if( tOpB == 0 ){
      mv->mem[indireccion1] |= vOpB;
    }else if( tOpB == 2 ){//b directo
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[indireccion1] |= mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[indireccion1] |= ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[indireccion1] |= mv->mem[indireccion2];
    }
    op = mv->mem[indireccion1];

  }else if( tOpA==2 ){ //! Op directo

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){
      mv->mem[direccion1] |= vOpB;
    }else if( tOpB == 2 ){//b directo
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[direccion1] |= mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[direccion1] |= ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[direccion1] |= mv->mem[indireccion2];
    }
    op = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro
    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 0 ){
      op = aux | vOpB;
    }else if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      op = aux | mv->mem[direccion2];
    }else if( tOpB == 1 ){
      op = aux | ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      op = aux | mv->mem[indireccion2];
    }
    AlmacenaEnRegistro(mv, vOpA, op,2);

  }
  modCC(mv, op);

}

void xor( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int op;
  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

    if( tOpA == 3){ //!Op indirecto

    indireccion1 = calculaIndireccion(*mv, vOpA);

    if( tOpB == 0 ){
      mv->mem[indireccion1] ^= vOpB;
    }else if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[indireccion1] ^= mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[indireccion1] ^= ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[indireccion1] ^= mv->mem[indireccion2];
    }
    op = mv->mem[indireccion1];

  }else if( tOpA==2 ){ //! Op directo

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){
      mv->mem[direccion1] ^= vOpB;
    }else if( tOpB == 2 ){//b directo
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[direccion1] ^= mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[direccion1] ^= ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[direccion1] ^= mv->mem[indireccion2];
    }
    op = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro
    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 0 ){
      op = aux ^ vOpB;
    }else if( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      op = aux ^ mv->mem[direccion2];
    }else if( tOpB == 1 ){
      op = aux ^ ObtenerValorDeRegistro(*mv, vOpB,2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      op = aux ^ mv->mem[indireccion2];
    }
    AlmacenaEnRegistro(mv, vOpA, op,2);

  }
  modCC(mv, op);

}

void slen( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int indireccion1, indireccion2;
  int direccion1, direccion2;
  char aux;
  int len;

  if( tOpA == 3 ){         //! opA indirecto

    indireccion1 = calculaIndireccion(*mv, vOpA);
    if( tOpB == 3 ){        //opB indirecto
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[indireccion1] = strlen(mv->mem[indireccion2]);
    }else if( tOpB == 2 ){   //opB directo
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[indireccion1] = strlen(mv->mem[direccion2]);
    }

  }else if( tOpA == 2 ){   //! opA directo

     direccion1 = calculaDireccion(*mv, vOpA);

     if( tOpB == 3 ){       //opB indirecto
       indireccion2 = calculaIndireccion(*mv, vOpB);
       mv->mem[direccion1] = strlen(mv->mem[indireccion2]);
     }else if( tOpB == 2 ){ //opB directo
       direccion2 = calculaDireccion(*mv, vOpB);
       mv->mem[direccion1] = strlen(mv->mem[direccion2]);
     }

  }else if( tOpA == 1 ){  //! opA de registro

     if ( tOpB == 3 ){    // opB indirecto
       indireccion2 = calculaIndireccion(*mv, vOpB);
       len = strlen( mv->mem[indireccion2] );
     }else if( tOpB == 2 ){  //opB directo
       direccion2 = calculaDireccion(*mv, vOpB);
       len = strlen( mv->mem[direccion2] );
     }

     AlmacenaEnRegistro(mv, vOpA, len, 2);

  }

}

void smov( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ) {

  int indireccion1, indireccion2;
  int direccion1, direccion2;

  if ( tOpA == 3 ){

    indireccion1 = calculaIndireccion(*mv, vOpA);
    if( tOpB == 3 ){
      indireccion2 = calculaIndireccion(*mv, vOpB);
      while( mv->mem[indireccion2] != '\0' ){
        mv->mem[indireccion1++] = mv->mem[indireccion2++];
      }
    }else if ( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      while( mv->mem[direccion2] != '\0' ){
        mv->mem[indireccion1++] = mv->mem[direccion2++];
      }
    }

  }else if( tOpA == 2 ){

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 3 ){
      indireccion2 = calculaIndireccion(*mv, vOpB);
      while( mv->mem[indireccion2] != '\0' ){
        mv->mem[direccion1++] = mv->mem[indireccion2++];
      }
    }else if ( tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      while( mv->mem[direccion1] != '\0' ){
        mv->mem[direccion1++] = mv->mem[direccion2++];
      }
    }

  }

}

void scmp( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ) {

  int indireccion1, indireccion2;
  int direccion1, direccion2;
  int rtado;

  if( tOpA == 3 ){

      indireccion1 = calculaIndireccion(*mv, vOpA);
      if( tOpB == 3 ){
        indireccion2 = calculaIndireccion(*mv, vOpB);
        rtado = strcmp(mv->mem[indireccion1],mv->mem[indireccion2]);
      }else if( tOpB == 2 ){
        direccion2 = calculaDireccion(*mv, vOpB);
        rtado = strcmp(mv->mem[indireccion1], mv->mem[direccion2]);
      }
      modCC(mv, rtado);

  }else if( tOpA == 2 ){

      direccion1 = calculaDireccion(*mv, vOpA);

      if( tOpB == 3 ){
        indireccion2 = calculaIndireccion(*mv, vOpB);
        rtado = strcmp(mv->mem[direccion1],mv->mem[indireccion2]);
      }else if( tOpB == 2 ){
        direccion2 = calculaDireccion(*mv, vOpB);
        rtado = strcmp(mv->mem[direccion1],mv->mem[direccion2]);
      }
      modCC(mv, rtado);
  }

}

void shl( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

  if( tOpA == 3 ){

    indireccion1 = calculaIndireccion(*mv, vOpA);

    if( tOpB == 0 ){
      mv->mem[indireccion1] = mv->mem[indireccion1] << vOpB;
    }else if (tOpB == 2 ){
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[indireccion1] = mv->mem[indireccion1] << mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[indireccion1] = mv->mem[indireccion1] << ObtenerValorDeRegistro(*mv, vOpB, 2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[indireccion1] = mv->mem[indireccion1] << mv->mem[indireccion2];
    }

  }else if( tOpA==2 ){ //! Op directo

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){ //shl [#] 10
      mv->mem[direccion1] = mv->mem[direccion1] << vOpB;
    }else if (tOpB == 2 ){ //shl [#] [#2]
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[direccion1] = mv->mem[direccion1] << mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[direccion1] = mv->mem[direccion1] << ObtenerValorDeRegistro(*mv, vOpB, 2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[direccion1] = mv->mem[direccion1] << mv->mem[indireccion2];
    }

    aux = mv->mem[direccion1];

  }else if( tOpA == 1 ){ //! Op de registro

     aux = ObtenerValorDeRegistro(*mv, vOpA, 2);

     if( tOpB == 0 ){ //shl EAX 10
      aux = aux << vOpB;
    }else if (tOpB == 2 ){ //shl EAX [#]
      direccion2 = calculaDireccion(*mv, vOpB);
      aux = aux << mv->mem[direccion2];
    }else if( tOpB == 1 ){
      aux = aux << ObtenerValorDeRegistro(*mv, vOpB, 2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      aux = aux << mv->mem[indireccion2];
    }
    AlmacenaEnRegistro(mv, vOpA, aux, 2);
  }

  modCC(mv, aux);

}

void shr( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int aux;
  int indireccion1, indireccion2;
  int direccion1, direccion2;

  if( tOpA == 3 ){

    indireccion1 = calculaIndireccion(*mv, vOpA);

    if( tOpB == 0 ){
      mv->mem[indireccion1] = mv->mem[indireccion1] >> vOpB;
    }else if (tOpB == 2 ){
      direccion2 = calculaDireccion(*mv,vOpB);
      mv->mem[indireccion1] = mv->mem[indireccion1] >> mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[indireccion1] = mv->mem[indireccion1] >> ObtenerValorDeRegistro(*mv, vOpB, 2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[indireccion1] = mv->mem[indireccion1] >> mv->mem[indireccion2];
    }

  }else if( tOpA==2 ){ //! Op directo

    direccion1 = calculaDireccion(*mv, vOpA);

    if( tOpB == 0 ){ //shl [#] 10
      mv->mem[direccion1] = mv->mem[direccion1] >> vOpB;
    }else if (tOpB == 2 ){ //shl [#] [#2]
      direccion2 = calculaDireccion(*mv, vOpB);
      mv->mem[direccion1] = mv->mem[direccion1] >> mv->mem[direccion2];
    }else if( tOpB == 1 ){
      mv->mem[direccion1] = mv->mem[direccion1] >> ObtenerValorDeRegistro(*mv, vOpB, 2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      mv->mem[direccion1] = mv->mem[direccion1] >> mv->mem[indireccion2];
    }

    aux = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro

     aux = ObtenerValorDeRegistro(*mv, vOpA, 2);

     if( tOpB == 0 ){ //shl EAX 10
      aux = aux >> vOpB;
    }else if (tOpB == 2 ){ //shl EAX [#]
      direccion2 = calculaDireccion(*mv, vOpB);
      aux = aux >> mv->mem[direccion2];
    }else if( tOpB == 1 ){
      aux = aux >> ObtenerValorDeRegistro(*mv, vOpB, 2);
    }else{
      indireccion2 = calculaIndireccion(*mv, vOpB);
      aux = aux >> mv->mem[indireccion2];
    }
    AlmacenaEnRegistro(mv, vOpA, aux, 2);
  }

  modCC(mv, aux);
}

void sysWrite( Mv* mv ){

    u32 celda, celdaMax, aux, aux2;
    int i;
    int num;
    char endl[3];

    //Los 12 bits menos significativos me daran informacion de modo de lectura
    aux = mv->reg[10] & 0x00000FFF;      //12 bits de AX
    celda = mv->reg[13];                 //EDX -> // Posicion de mem inicial desde donde empiezo la lect
    celdaMax = mv->reg[12] & 0x0000FFFF; //CX (Cuantas posiciones de mem como max)
     for( i = celda; i < celda + celdaMax ; i++ ){

       if( (aux & 0x800) == 0 ){ //prompt
         printf("[%04d]:",i);
       }

        if( (aux & 0x100) == 0x100 )
          strcpy(endl,"");
        else
          strcpy(endl,"\n");


        if ( (aux & 0x0F0) == 0x010 ){ //Imprime caracter
          int aux2 = mv->mem[mv->reg[0] + i] & 0xFF; // 1er byte
          if( aux2 != 127 && aux2 >= 32 && aux2 <= 255 ) //Verifico rango del ascii
            printf("%c%s ", aux2, endl );
          else
            printf(".%s",endl);
        }else if( (aux & 0x00F) == 0x001 ) //Imprime decimal
          printf("%d%s", mv->mem[mv->reg[0] + i], endl);
        else if( (aux & 0x00F) == 0x004) //Imprime octal
          printf("%O%s", mv->mem[mv->reg[0] + i], endl);
        else if( (aux & 0x00F) == 0x008) //Imprime hexadecimal
          printf("%X%s", mv->mem[mv->reg[0] + i], endl);

      }
    printf("\n");
}

void sysRead( Mv* mv ){

    u32 celda, celdaMax, aux;
    int i;
    int num;
    int prompt = 0;

    //Los 12 bits menos significativos me daran informacion de modo de lectura
    aux = mv->reg[0xA] & 0x00000FFF;      //12 bits de AX
    celda = mv->reg[0xE];                 //EDX -> // Posicion de mem inicial desde donde empiezo la lect
    celdaMax = mv->reg[0xC] & 0x0000FFFF; //CX (Cuantas posiciones de mem como max)
    printf("\n");
    //Prendido octavo bit -> Leo string, guardo caracter a caracter
    if( (aux & 0x100) == 0x100 ){

      char string[1000]="";

      if( (aux & 0x800) == 0x000 ) //prompt
        printf("[%04d]:",celda);

      scanf("%s",string);

      i = 0;
      while( (i < strlen(string)) && (string[i] != "") ){
        mv->mem[ mv->reg[0] + celda + i ] = string[i];
        i++;
      }

    }else

      for( i = celda; i < celda + celdaMax ; i++ ){

        if( (aux & 0x800) == 0x00 ) //prompt
          printf("[%04d]:",i);

        if( (aux & 0x00F) == 0x001 )     //Interpreta decimal
          scanf("%d", &num);
        else if( (aux & 0x00F) == 0x004) //Interpreta octal
          scanf("%O", &num);
        else if( (aux & 0x00F) == 0x008) //Interpreta hexadecimal
          scanf("%X", &num);

        mv->mem[mv->reg[0] + i] = num;

      }
}

void sysStringRead(Mv* mv){

    int direccion;
    int charMax;
    int bitPrompt;
    int HEreg, tamSegmento;
    int i;

    char *str;

    //Recupero EDX y calculo la direccion relativa al segmento correspondiente
    direccion = calculaDireccion(*mv, mv->reg[0xE]);

    //Recupero la cantidad max de caracteres a leer
    charMax = mv->reg[0xC];

    str = (char*)malloc(charMax*sizeof(char));

    //Recupero bit de prompt
    bitPrompt = (mv->reg[0xA] & 0x800) >> 11;

    if( bitPrompt == 0 )
      printf("[%d]: ", direccion);

    fgets(str, charMax, stdin);

    //Obtengo la parte alta del registro E (me indica el segmento)
    HEreg = mv->reg[0xE] >> 16;

    //Obtengo el tamaño del segmento
    tamSegmento = mv->reg[HEreg] >> 16;

    if( strlen(str) > (tamSegmento - direccion) ){
      printf("Segmentation fault");
      exit(-1);
    }else{
      i = 0;
      while( str[i] != '\0' ){
        mv->mem[direccion++] = str[i++];
      }
    }

}

void sysStringWrite(Mv *mv){

  char* str;
  int direccion;
  int bitPrompt, bitEndl;

  //Recupero posicion de inicio de string
  str = (char*) calculaDireccion(*mv, mv->reg[0xE]);

  //Recupero bit de prompt
  bitPrompt = (mv->reg[0xA] & 0x800) >> 11;

  //Recupero bit de endline
  bitEndl = (mv->reg[0xA] & 0x100) >> 8;

  if( bitPrompt == 0 )
    printf("[%d]: ", direccion);

  puts(str);

  if( bitEndl == 0 )
    printf("\n");

}

void sysCls(){

  system("cls");

}

void sys( Mv* mv, int tOpA, int vOpA ,char mnem[],char* argv[],int argc){

  if( vOpA == 0x2 )      //! Write
    sysWrite( mv );
  else if( vOpA == 0x1 ) //! Read
    sysRead( mv );
  else if( vOpA == 0xF ) //! BreakPoint
    sysBreakpoint(mv,argv,argc,mnem,0);
  else if( vOpA == 0x3 ) //! StringRead
    sysStringRead(mv);
  else if( vOpA == 0x4)  //! StringWrite
    sysStringWrite(mv);
  else if( vOpA == 0x7)  //! ClearScreen
    sysCls();

}

//Recorre los argumentos ingresados desde el ejecutable
//y si existe retorna 1
int apareceFlag(char* argv[],int argc, char* flag ){

  int i = 0;
  int aparece = 0;

  while( i < argc && aparece == 0){
    if( strcmp(argv[i], flag) == 0 )
      aparece = 1;
    i++;
  }
  return aparece;

}

void sysBreakpoint( Mv* mv, char* argv[],int argc,char mnem[],int llamaP){ //! Sin probar

  char aux[10]={""},opA[7]={""},opB[7]={""}; //max 4096#9862
  int i=0,j=0,noNum, num,tOpB=0,IPinicial;

  if( !llamaP )
    sysC( argv, argc );

  if( apareceFlag( argv, argc, "-b" ) ){
    printf("[%04d] cmd: ", mv->reg[5] );//print de IP
    printf("Ingresa un comando: ");
    //Scanf detiene la lecutra cuando encuentra un espacio en blanco
    //gets me permite leer hasta encontrar un salto de linea
    gets(aux);
    if( aux[0] == 'p' ){
      step(mv,argv,argc);
      sysBreakpoint(mv, argv,argc,mnem,1);
    }else if( aux[0] == 'r' ){
    }
      else{ //Numero entero positivo
        char *token = strtok(aux, " ");
        int cantArg = 0;
        int dms[2];
        while ( token != NULL ) {
          dms[cantArg] = atoi(token);
          cantArg++;
          token = strtok(NULL, " ");
        }
        if (cantArg == 1) {
            printf("[%04d] %08X %d\n", dms[0], mv->mem[dms[0]],mv->mem[dms[0]]);
        } else { //dos argumentos numericos
            if (dms[0] < dms[1]) {
                for(int i = dms[0]; i <= dms[1]; i++ ){
                    printf("[%04d] %08X %d\n", i, mv->mem[i],mv->mem[i]);
                }
            }
        }
    }
  }

   if ( apareceFlag( argv, argc, "-d" ) ){

    IPinicial=mv->reg[5];
    i=0;
    while(i<10 && IPinicial+i<mv->reg[0]){
      falsoStep(mv,opA,opB,i); //Muestro los operandos y paso a la siguiente opeeracion sin ejecutar
      i++;
    }
    mv->reg[5]=IPinicial;
    printf("\nRegistros: \n");
    printf("DS =    %06d  |              |                 |                |\n",mv->reg[0]);
    printf("                | IP =   %06d|                 |                |\n",mv->reg[5]);
    printf("CC  =    %06d | AC =   %06d|EAX =     %06d |EBX =     %06d|\n",mv->reg[8],mv->reg[9],mv->reg[10],mv->reg[11]);
    printf("ECX =   %06d  |EDX =   %06d|EEX =     %06d |EFX =     %06d|\n",mv->reg[12],mv->reg[13],mv->reg[14],mv->reg[15]);

  }
}

//Step utilizado para poder realizar en el sys %F -d
//Es un step pero sin la ejecucion
void falsoStep(Mv* mv,char opA[],char opB[],int i){ //! Sin probar

  int instr;
  int numOp, codInstr;
  char mnem[5];
  int tOpA, tOpB;
  int vOpA, vOpB;

  instr = mv->mem[mv->reg[5]]; //fetch
  DecodificarInstruccion(instr,&numOp,&codInstr,mnem); //Decodifica el numero de operandos de la instruccion
  mv->reg[5]++;
  DecodificarOperacion(instr,&tOpA,&tOpB,&vOpA,&vOpB,numOp);
  disassembler(mv, tOpA, tOpB, vOpA, vOpB, opA, opB, numOp);
  if(i==0)
    printf("\n>[%04d]:  %x   %d: %s   %s,%s",mv->reg[5]-1,instr,mv->reg[5],mnem,opA,opB);
  else
    printf("\n [%04d]:  %x   %d: %s   %s,%s",mv->reg[5]-1,instr,mv->reg[5],mnem,opA,opB);

}

//Sys%F -c -> Ejecuta el clearscreen
void sysC(char* argv[], int argc){

  if( apareceFlag(argv, argc, "-c") )
    system("cls");

}

void jmp( Mv* mv,int tOpA,int vOpA){

      int indireccion;
      int direccion;

     if( tOpA == 0 ){//Inmediato
       mv->reg[5] = vOpA;
     }
     else if( tOpA == 1 ){//De registro
       mv->reg[5] = ObtenerValorDeRegistro(*mv,vOpA, 2);
     }
     else if( tOpA == 2 ){//Directo
       direccion = calculaDireccion(*mv, vOpA);
       mv->reg[5] = mv->mem[direccion];
     }
     else{
       indireccion = calculaIndireccion(*mv, vOpA);
       mv->reg[5] = mv->mem[indireccion];
     }

}

//Devuelve segun el tOp el valor dentro del registro, memoria o bien como cte
int DevuelveValor(Mv mv,int tOpA,int vOpA, int numOp){

    int aux;
    int direccion1;

    if( tOpA == 0 ) //Inmediato
        aux = vOpA;
    else if( tOpA == 1 )//De Registro
          aux = ObtenerValorDeRegistro(mv,vOpA,numOp);
        else{
          direccion1 = calculaDireccion(mv, vOpA);
          aux = mv.mem[direccion1];
        }

    return aux;
}

void rnd(Mv *mv, int tOpA, int vOpA){

  int direccion;

  int aux;
  if( tOpA == 0 )         //Inmediato
    aux = vOpA;
  else
    if (tOpA == 1)        //De registro
      aux = ObtenerValorDeRegistro(*mv, vOpA, 1);
    else
      if (tOpA == 2) {    //Directo
        direccion = calculaDireccion(*mv, vOpA);
        aux = mv->mem[direccion];
      }else
        if( tOpA == 3 ){  //Indirecto
          direccion = calculaIndireccion(*mv, vOpA);
          aux = mv->mem[direccion];
        }

  //Cargo en AC el valor aleatorio entre 0 y el valor del operando
  mv->reg[9] = rand() % (aux + 1);

}

