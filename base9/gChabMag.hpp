#ifndef GCHABMAG_H
#define GCHABMAG_H

#include <string>

const int N_CHAB_FILTS     = 8;
const int N_CHAB_Z         = 4;    /* number of metallicities in Chaboyer-Dotter isochrones */
const int N_CHAB_Y         = 5;    /* number of He abundances in Chaboyer-Dotter isochrones */
const int N_CHAB_AGES      = 19;   /* number of ages in Chaboyer-Dotter isochonres */
const int MAX_CHAB_ENTRIES = 280;

void loadChaboyer (std::string path, int filterSet);
double deriveChabAgbTip (double newFeH, double newY, double newAge);
double getChaboyerMags (double zamsMass);
double wdPrecLogAgeChaboyer (double FeH, double thisY, double zamsMass);
#endif
