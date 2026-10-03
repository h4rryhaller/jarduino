#ifndef PROGRAMADOR_H
#define PROGRAMADOR_H

// Riego automático según el horario de cada zona. Si el ESP arranca a mitad
// de una ventana de riego, riega lo que queda de ella. Si alguien detiene un
// riego automático, no se vuelve a abrir hasta el día siguiente, salvo que
// se cambie su horario.
void programadorTick();

#endif
