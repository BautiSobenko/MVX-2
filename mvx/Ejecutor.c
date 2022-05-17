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
      "DS", " "," "," "," ",
      "IP"," "," ","CC","AC",
      "A","B","C","D","E","F"};

typedef unsigned int u32;

typedef struct Mv{

  int reg[16];

  int mem[4096];

}Mv;

//! DECLARACION DE PROTOTIPOS

u32 bStringtoInt(char* string);
void cargaMemoria(Mv* mv, char *argv[]);
int examinaHeader(FILE* arch);
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

int main(int argc, char *argv[]) {

  Mv mv;
  mv.reg[5] = 0;
  mv.reg[0] = 0;

  sysC(argv, argc);

  cargaMemoria(&mv, argv);

  printf("Codigo: \n");
  do{
    step(&mv,argv,argc);
  }while( (0 <= mv.reg[5]) && ( mv.reg[5] < mv.reg[0] ));

  return 0;

}

void cargaMemoria(Mv* mv, char *argv[]){

  char linea[256];
  int inst;

  FILE* arch = fopen(argv[1],"r");

  if( arch ){
    if( examinaHeader(arch) ){
      while( fgets(linea, 256, arch) ){
        mv->mem[mv->reg[0]] = bStringtoInt(linea);
        mv->reg[0]++;
      }
      fclose(arch);
    }else
      printf("[!] Carga de memoria fallida, capacidad de memoria excedida. ");
  }else
    printf("[!] Carga de memoria fallida, no se pudo leer el archivo. ");

}

//Lee los bloques del header y verifica si el archivo puede ser leido
int examinaHeader(FILE* arch){

  char linea[256];
  int lc;
  int i;

  if( !arch ) return 0;

  fgets(linea, 256, arch); //Bloque 0 del header
  fgets(linea, 256, arch); //Bloque 1 del Header (Tamano del codigo)
  lc = bStringtoInt(linea);
  if( lc <= 4096 ){        //Si la longitud del codigo NO EXCEDE mi cap. de memoria
    for( i = 1 ; i <= 4 ; i++)
      fgets(linea, 256, arch); //Leo los 4 bloques restantes
    return 1;
  }
  else{
    fclose(arch);
    return 0;
  }

}

//Muestra todas las instrucciones cargadas en memoria
void muestraCS(Mv mv){

  int i;
  for( i = 0; i < mv.reg[0]; i++){
    printf("%08X\n",mv.mem[i]);
  }

}

//Verifica si un numero es negativo, y en caso de serlo, agrega las F faltantes a su izquierda
int complemento2(int vOp, int numOp ){

  int c2 = vOp;

  if( numOp == 2 && c2 < 0 ){
    if( (vOp & 0x800) == 0x800 ){ // Caso 12 bits
      c2 = vOp | 0xFFFFF000;
    }else
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
      case 0xA: //rnd
        rnd(mv, tOpA, vOpA);
        break;
      case 0xB://!not
        not( mv, vOpA, tOpA );
        break;
    }else{     //!stop
      mv->reg[5]=mv->reg[0];
  }

}

void not( Mv* mv, int vOp, int tOp ){

  int valorNeg = ~DevuelveValor(*mv, tOp, vOp);

  if( tOp == 1 ){ //De registro
    AlmacenaEnRegistro( mv, vOp, valorNeg, 1 );
  }else if( tOp == 2 ){ //Directo
    mv->mem[mv->reg[0] + vOp] = valorNeg;
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

void mov(Mv* mv,int tOpA,int tOpB,int vOpA,int vOpB){

  //Variables de op de reg.
  int codReg;
  int sectorReg;

  if( tOpA==2 ){ //!OpA -> directo -> [10]

    if( tOpB == 0 ) // mov [10] 10
      mv->mem[mv->reg[0] + vOpA] = vOpB;
    else
    if( tOpB == 1 ) // mov [10] AX
      mv->mem[mv->reg[0] + vOpA] = ObtenerValorDeRegistro(*mv,vOpB,2);
    else          // mov [vOpA] [vOpB]
      mv->mem[mv->reg[0] + vOpA] = mv->mem[mv->reg[0] + vOpB];

  }else if( tOpA == 1 ){          //!Op -> De registro -> EAX

      if( tOpB == 0 ){ // mov AX 10
        AlmacenaEnRegistro(mv, vOpA, vOpB, 2);
      }else
      if( tOpB == 2){ // mov AX [10]
        AlmacenaEnRegistro( mv, vOpA, mv->mem[mv->reg[0] + vOpB], 2 );
      }else{ //mov AX EAX
        AlmacenaEnRegistro( mv, vOpA, ObtenerValorDeRegistro(*mv,vOpB,2), 2 );
      }
    }
}

void add(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB){

  int aux;

  if( tOpA==2 ){ //! tOpA -> Directo [vOpA]

    if( tOpB == 0 ){ // add [10] 10
      mv->mem[mv->reg[0] + vOpA] += vOpB;
    }else if( tOpB == 2){ //add [vOpA] [vOpB]
      mv->mem[mv->reg[0] + vOpA] += mv->mem[mv->reg[0] + vOpB];
    }else{//directo y reg
     mv->mem[mv->reg[0] + vOpA] += ObtenerValorDeRegistro(*mv,vOpB,2);
    }
    modCC(mv, mv->mem[mv->reg[0] + vOpA]);

  }else
    if( tOpA == 1 ){ //! tOpA -> DeRegistro EAX

      aux = ObtenerValorDeRegistro(*mv,vOpA,2);
      if( tOpB == 0 ){ // add reg 10
        aux += vOpB;
      }else if( tOpB == 2){ //add reg [vOpB]
        aux += mv->mem[mv->reg[0] + vOpB];
      }else{//reg y reg
        aux += ObtenerValorDeRegistro(*mv,vOpB,2);
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

  if( tOpA==2 ){ //! tOpA -> Directo [vOpA]

    if( tOpB == 0 ){ // add [10] 10
      mv->mem[mv->reg[0] + vOpA] *= vOpB;
    }else if( tOpB == 2){ //add [vOpA] [vOpB]
      mv->mem[mv->reg[0] + vOpA] *= mv->mem[mv->reg[0] + vOpB];
    }else{//directo y de reg
     mv->mem[mv->reg[0] + vOpA] *= ObtenerValorDeRegistro(*mv,vOpB,2);
    }
    modCC(mv, mv->mem[mv->reg[0] + vOpA]);

  }else
    if( tOpA == 1 ){ //! tOpA -> DeRegistro EAX

      aux = ObtenerValorDeRegistro(*mv,vOpA,2);
      if( tOpB == 0 ){ // add reg 10
        aux *= vOpB;
      }else if( tOpB == 2){ //add reg [vOpB]
        aux *= mv->mem[mv->reg[0] + vOpB];
      }else{// reg reg
        aux *= ObtenerValorDeRegistro(*mv,vOpB,2);
      }
      AlmacenaEnRegistro(mv, vOpA, aux, 2);
      modCC(mv, aux);
    }
}

void sub(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB){

  int aux;

  if( tOpA==2 ){ //! tOpA -> Directo [vOpA]

    if( tOpB == 0 ){ // add [10] 10
      mv->mem[mv->reg[0] + vOpA] -= vOpB;
    }else if( tOpB == 2){ //add [vOpA] [vOpB]
      mv->mem[mv->reg[0] + vOpA] -= mv->mem[mv->reg[0] + vOpB];
    }else{
     mv->mem[mv->reg[0] + vOpA] -= ObtenerValorDeRegistro(*mv,vOpB,2);
    }
    modCC(mv, mv->mem[mv->reg[0] + vOpA]);

  }else
    if( tOpA == 1 ){ //! tOpA -> DeRegistro EAX

      aux = ObtenerValorDeRegistro(*mv,vOpA,2);
      if( tOpB == 0 ){ // add [10] 10
        aux -= vOpB;
      }else if( tOpB == 2){ //add [vOpA] [vOpB]
        aux -= mv->mem[mv->reg[0] + vOpB];
      }else{
        aux -= ObtenerValorDeRegistro(*mv,vOpB,2);
      }
      AlmacenaEnRegistro(mv, vOpA, aux, 2);
      modCC(mv, aux);
    }
}

void divi(Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB){

  int aux;
  int auxB;
  int cero = 0;

  if( tOpA==2 ){ //! tOpA -> Directo [vOpA]

    if( tOpB == 0 ){ // add [10] 10
      if( vOpB ){
        mv->reg[9] = mv->mem[mv->reg[0] + vOpA] % vOpB;
        mv->mem[mv->reg[0] + vOpA] /= vOpB;
      }
      else
        printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }else if( tOpB == 2){ //add [vOpA] [vOpB]
      if( mv->mem[mv->reg[0] + vOpB] ){
        mv->reg[9] = mv->mem[mv->reg[0] + vOpA] % mv->mem[mv->reg[0] + vOpB];
        mv->mem[mv->reg[0] + vOpA] /= mv->mem[mv->reg[0] + vOpB];
      }
      else
        printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }else{
       auxB = ObtenerValorDeRegistro(*mv,vOpB,2);
       if( auxB ){
         mv->reg[9] = mv->mem[mv->reg[0] + vOpA] % auxB;
         mv->mem[mv->reg[0] + vOpA] /= auxB;
       }
       else
         printf("\n[%04d] [!] Division por cero",mv->reg[5]-1);
    }
    modCC(mv, mv->mem[mv->reg[0] + vOpA]);
  }else
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
        if( mv->mem[mv->reg[0] + vOpB] ){
          mv->reg[9] = aux % mv->mem[mv->reg[0] + vOpB];
          aux /= mv->mem[mv->reg[0] + vOpB];
        }
        else
          cero = 1;
      }else{
        auxB = ObtenerValorDeRegistro(*mv,vOpB,2);
        if( auxB ){
          mv->reg[9] = aux % auxB;
          aux /= auxB;
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

  if( tOpA==2 ){ //! Op directo

    aux = mv->mem[mv->reg[0] + vOpA];
    if( tOpB == 2 ){
      mv->mem[mv->reg[0] + vOpA] = mv->mem[mv->reg[0] + vOpB];
      mv->mem[mv->reg[0] + vOpB] = aux;
    }else if( tOpB == 2 ){
      mv->mem[mv->reg[0] + vOpA] = ObtenerValorDeRegistro(*mv, vOpB, 2);
      AlmacenaEnRegistro(mv, vOpB, aux,2);
    }

  }else if( tOpA ==1 ){ //!De registro

    aux = ObtenerValorDeRegistro(*mv, vOpA,2);
    if( tOpB == 2 ){
      AlmacenaEnRegistro(mv,vOpA,mv->mem[mv->reg[0] + vOpB],2);
      mv->mem[mv->reg[0] + vOpB] = aux;
    }else if( tOpB == 1 ){
      AlmacenaEnRegistro(mv, vOpA, ObtenerValorDeRegistro(*mv, vOpB,2),2);
      AlmacenaEnRegistro(mv, vOpB, aux,2);
    }

  }

}

void cmp( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int sub;
  int aux;

  if( tOpA == 0 ){                 //! Op inmediato

    if( tOpB == 0 ){               // cmp 10, 11
      sub = vOpA - vOpB;
    }else if( tOpB == 2 ){         // cmp 10, [11]
      sub = vOpA - mv->mem[mv->reg[0] + vOpB];
    }else{                         // cmp 10, EAX
      sub = vOpA - ObtenerValorDeRegistro(*mv, vOpB,2);
    }

  }else if( tOpA==2 ){           //! Op directo

     aux = mv->mem[mv->reg[0] + vOpA];

     if( tOpB == 0 ){              // cmp [10], 11
      sub = aux - vOpB;
    }else if( tOpB == 2 ){         // cmp [10], [11]
      sub = aux - mv->mem[mv->reg[0] + vOpB];
    }else{                         // cmp [10], EAX
      sub = aux - ObtenerValorDeRegistro(*mv, vOpB,2);
    }

  }else{                           //! Op de registro
    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 0 ){               // cmp EAX, 11
      sub = aux - vOpB;
    }else if( tOpB == 2 ){         // cmp EAX, [11]
      sub = aux - mv->mem[mv->reg[0] + vOpB];
    }else{                         // cmp EAX, AX
      sub = aux - ObtenerValorDeRegistro(*mv, vOpB,2);
    }

  }
  modCC(mv, sub);

}

void and( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int op;
  int aux;

  if( tOpA==2 ){ //! Op directo

    if( tOpB == 0 ){
      mv->mem[mv->reg[0] + vOpA] &= vOpB;
    }else if( tOpB == 2 ){
      mv->mem[mv->reg[0] + vOpA] &= mv->mem[mv->reg[0] + vOpB];
    }else{
      mv->mem[mv->reg[0] + vOpA] &= ObtenerValorDeRegistro(*mv, vOpB,2);
    }
    op = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro
    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 0 ){
      op = aux & vOpB;
    }else if( tOpB == 2 ){
      op = aux & mv->mem[mv->reg[0] + vOpB];
    }else{
      op = aux & ObtenerValorDeRegistro(*mv, vOpB,2);
    }
    AlmacenaEnRegistro(mv, vOpA, op,2);

  }
  modCC(mv, op);

}

void or( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int op;
  int aux;

  if( tOpA==2 ){ //! Op directo

    if( tOpB == 0 ){
      mv->mem[mv->reg[0] + vOpA] |= vOpB;
    }else if( tOpB == 2 ){//b directo
      mv->mem[mv->reg[0] + vOpA] |= mv->mem[mv->reg[0] + vOpB];
    }else{//de registro
      mv->mem[mv->reg[0] + vOpA] |= ObtenerValorDeRegistro(*mv, vOpB,2);
    }
    op = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro
    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 0 ){
      op = aux | vOpB;
    }else if( tOpB == 2 ){// b directo
      op = aux | mv->mem[mv->reg[0] + vOpB];
    }else{// b registro
      op = aux | ObtenerValorDeRegistro(*mv, vOpB,2);
    }
    AlmacenaEnRegistro(mv, vOpA, op,2);

  }
  modCC(mv, op);

}

void xor( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int op;
  int aux;

  if( tOpA==2 ){ //! Op directo

    if( tOpB == 0 ){
      mv->mem[mv->reg[0] + vOpA] ^= vOpB;
    }else if( tOpB == 2 ){
      mv->mem[mv->reg[0] + vOpA] ^= mv->mem[mv->reg[0] + vOpB];
    }else{
      mv->mem[mv->reg[0] + vOpA] ^= ObtenerValorDeRegistro(*mv, vOpB,2);
    }
    op = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro
    aux = ObtenerValorDeRegistro(*mv, vOpA,2);

    if( tOpB == 0 ){
      op = aux ^ vOpB;
    }else if( tOpB == 2 ){
      op = aux ^ mv->mem[mv->reg[0] + vOpB];
    }else{
      op = aux ^ ObtenerValorDeRegistro(*mv, vOpB,2);
    }
    AlmacenaEnRegistro(mv, vOpA, op,2);

  }
  modCC(mv, op);

}

void shl( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int aux;

  if( tOpA==2 ){ //! Op directo

    if( tOpB == 0 ){ //shl [#] 10
      mv->mem[mv->reg[0] + vOpA] = mv->mem[mv->reg[0] + vOpA] << vOpB;
    }else if (tOpB == 2 ){ //shl [#] [#2]
      mv->mem[mv->reg[0] + vOpA] = mv->mem[mv->reg[0] + vOpA] << mv->mem[mv->reg[0] + vOpB];
    }else{
      mv->mem[mv->reg[0] + vOpA] = mv->mem[mv->reg[0] + vOpA] << ObtenerValorDeRegistro(*mv, vOpB, 2);
    }

    aux = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro
     aux = ObtenerValorDeRegistro(*mv, vOpA, 2);

     if( tOpB == 0 ){ //shl EAX 10
      aux = aux << vOpB;
    }else if (tOpB == 2 ){ //shl EAX [#]
      aux = aux << mv->mem[mv->reg[0] + vOpB];
    }else{
      aux = aux << ObtenerValorDeRegistro(*mv, vOpB, 2);
    }
    AlmacenaEnRegistro(mv, vOpA, aux, 2);
  }
  modCC(mv, aux);

}

void shr( Mv* mv, int tOpA, int tOpB, int vOpA, int vOpB ){

  int aux;

  if( tOpA==2 ){ //! Op directo

    if( tOpB == 0 ){ //shl [#] 10
      mv->mem[mv->reg[0] + vOpA] = mv->mem[mv->reg[0] + vOpA] >> vOpB;
    }else if (tOpB == 2 ){ //shl [#] [#2]
      mv->mem[mv->reg[0] + vOpA] = mv->mem[mv->reg[0] + vOpA] >> mv->mem[mv->reg[0] + vOpB];
    }else{
      mv->mem[mv->reg[0] + vOpA] = mv->mem[mv->reg[0] + vOpA] >> ObtenerValorDeRegistro(*mv, vOpB, 2);
    }

    aux = mv->mem[mv->reg[0] + vOpA];

  }else if( tOpA == 1 ){ //! Op de registro
     aux = ObtenerValorDeRegistro(*mv, vOpA, 2);

     if( tOpB == 0 ){ //shl EAX 10
      aux = aux >> vOpB;
    }else if (tOpB == 2 ){ //shl EAX [#]
      aux = aux >> mv->mem[mv->reg[0] + vOpB];
    }else{
      aux = aux >> ObtenerValorDeRegistro(*mv, vOpB, 2);
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
    aux = mv->reg[10] & 0x00000FFF;      //12 bits de AX
    celda = mv->reg[13];                 //EDX -> // Posicion de mem inicial desde donde empiezo la lect
    celdaMax = mv->reg[12] & 0x0000FFFF; //CX (Cuantas posiciones de mem como max)
    printf("\n");
    //Prendido octavo bit -> Leo string, guardo caracter a caracter
    if( (aux & 0x100) == 0x100 ){

      char string[1000]="";

      if( (aux & 0x800) == 0 ) //prompt
        printf("[%04d]:",celda);

      scanf("%s",string);

      i = 0;
      while( i < strlen(string) && string[i] != "" ){
        mv->mem[ mv->reg[0] + celda + i ] = string[i];
        i++;
      }

    }else

      for( i = celda; i < celda + celdaMax ; i++ ){

        if( (aux & 0x800) == 0 ){ //prompt
          printf("[%04d]:",i);
        }

        if( (aux & 0x00F) == 0x001 ) //Interpreta decimal
          scanf("%d", &num);
        else if( (aux & 0x00F) == 0x004) //Interpreta octal
          scanf("%O", &num);
        else if( (aux & 0x00F) == 0x008) //Interpreta hexadecimal
          scanf("%X", &num);

        mv->mem[mv->reg[0] + i] = num;

    }
}

void sys( Mv* mv, int tOpA, int vOpA ,char mnem[],char* argv[],int argc){

  if( vOpA == 2 ){ //! WRITE :D
    sysWrite( mv );
  }else if( vOpA == 1 ){ //! READ
    sysRead( mv );
  }else if( vOpA == 0xF ){ //! BREAKPOINT
    sysBreakpoint(mv,argv,argc,mnem,0);
  }

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

     if(tOpA==0){//Inmediato
        mv->reg[5]=vOpA;
     }else
        if(tOpA==1){//De registro
            mv->reg[5]=ObtenerValorDeRegistro(*mv,vOpA, 2);
        }else{//Directo
            mv->reg[5]=mv->mem[mv->reg[0]+vOpA];
        }
}

//Devuelve segun el tOp el valor dentro del registro, memoria o bien como cte
int DevuelveValor(Mv mv,int tOpA,int vOpA, int numOp){

    int aux;

    if( tOpA == 0 ) //Inmediato
        aux = vOpA;
    else if( tOpA == 1 )//De Registro
          aux = ObtenerValorDeRegistro(mv,vOpA,numOp);
        else
          aux = mv.mem[mv.reg[0] + vOpA];

    return aux;
}

void rnd(Mv *mv, int tOpA, int vOpA){

  int aux = vOpA;

  if (tOpA == 1) { //de registro
    aux = ObtenerValorDeRegistro(*mv, vOpA, 1);
  } else if (tOpA == 2) { //directo
    aux = mv->mem[mv->reg[0] + vOpA];
  }

  mv->reg[9] = rand() % (aux + 1);
}

