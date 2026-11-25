/******************************************/
/*
  duplex.cpp
  by Gary P. Scavone, 2006-2019.

  This program opens a duplex stream and passes
  input directly through to the output.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>

/*
typedef char MY_TYPE;
#define FORMAT RTAUDIO_SINT8
*/

typedef double MY_TYPE;
#define FORMAT RTAUDIO_FLOAT64

// Chemin relatif au dossier depuis lequel on lance ./duplex
#define FILENAME "../../audio_files/F01_a3_s100_v04.bin"

#define USE_FILE 1          // 1 = entrée fichier, 0 = micro
#define DUMP_SECONDS 10     // durée enregistrée (en secondes)

#define DUMP_IN_FILENAME  "files/test_01.bin"

#define DUMP_OUT_FILENAME "files/test_01_out.bin"

/*
typedef S24 MY_TYPE;
#define FORMAT RTAUDIO_SINT24

typedef signed long MY_TYPE;
#define FORMAT RTAUDIO_SINT32

typedef float MY_TYPE;
#define FORMAT RTAUDIO_FLOAT32

typedef double MY_TYPE;
#define FORMAT RTAUDIO_FLOAT64
*/

// Structure d'échange entre main() et le callback inout()
typedef struct {
  unsigned int nChannels;     // nombre de canaux
  unsigned int bufferFrames;  // taille du buffer en trames
  unsigned int bufferBytes;   // taille du buffer en octets
  unsigned int fs;            // fréquence d'échantillonnage
  double       *fileData;     // échantillons du fichier chargés en mémoire
  unsigned long fileLength;   // nombre d'échantillons du fichier
  unsigned long readIndex;    // tête de lecture (conservée entre deux appels de inout)

  // Buffers de travail mono (taille bufferFrames)
  double *in;
  double *out;

  // Buffers de dump
  int     nDump;        // taille des buffers de dump
  double *dumpIn;       // enregistrement de l'entrée
  int     indDumpIn;    // tête d'écriture de dumpIn
  double *dumpOut;      // Q11 : enregistrement de la sortie
  int     indDumpOut;   // Q11 : tête d'écriture de dumpOut
} UserData;

void usage( void ) {
  // Error function in case of incorrect command-line
  // argument specifications
  std::cout << "\nuseage: duplex N fs <iDevice> <oDevice> <iChannelOffset> <oChannelOffset>\n";
  std::cout << "    where N = number of channels,\n";
  std::cout << "    fs = the sample rate,\n";
  std::cout << "    iDevice = optional input device index to use (default = 0),\n";
  std::cout << "    oDevice = optional output device index to use (default = 0),\n";
  std::cout << "    iChannelOffset = an optional input channel offset (default = 0),\n";
  std::cout << "    and oChannelOffset = optional output channel offset (default = 0).\n\n";
  exit( 0 );
}

unsigned int getDeviceIndex( std::vector<std::string> deviceNames, bool isInput = false )
{
  unsigned int i;
  std::string keyHit;
  std::cout << '\n';
  for ( i=0; i<deviceNames.size(); i++ )
    std::cout << "  Device #" << i << ": " << deviceNames[i] << '\n';
  do {
    if ( isInput )
      std::cout << "\nChoose an input device #: ";
    else
      std::cout << "\nChoose an output device #: ";
    std::cin >> i;
  } while ( i >= deviceNames.size() );
  std::getline( std::cin, keyHit );  // used to clear out stdin
  return i;
}

double streamTimePrintIncrement = 1.0; // seconds
double streamTimePrintTime = 1.0; // seconds

double *loadBinaryFile(const char *filename, unsigned long *length)
{
  // 1. Ouverture en lecture binaire
  FILE *f = fopen( filename, "rb" );
  if ( f == NULL ) {
    std::cerr << "Erreur : impossible d'ouvrir " << filename << std::endl;
    return NULL;
  }

  // 2-4. Taille : aller à la fin, lire la position, revenir au début
  fseek( f, 0, SEEK_END );
  long nBytes = ftell( f );
  fseek( f, 0, SEEK_SET );
  *length = (unsigned long) nBytes / sizeof(double);

  // Allocation
  double *data = (double *) malloc( (*length) * sizeof( double ) );
  if ( data == NULL ) {
    fclose(f);
    return NULL;
  }

  // 5. Lecture
  size_t nRead = fread( data, sizeof( double ), *length, f );
  fclose( f );
  if (nRead != *length){
    free(data);
    return NULL;
  }

  return data;
}

int write_buff_dump(double* buff, const int n_buff, double* buff_dump, const int n_buff_dump, int* ind_dump) {

  int i = 0;
  for (i = 0; i < n_buff; i++) 
  {
    if (*ind_dump < n_buff_dump) 
    {
      buff_dump[*ind_dump] = buff[i];
      (*ind_dump)++;
    } else 
    {
      break;
    }
  }

  return i;
}

int writeBinaryFile( const char *filename, double *data, int n )
{
  FILE *f = fopen( filename, "wb" );
  if ( f == NULL ) {
    std::cerr << "Erreur : impossible de créer " << filename << std::endl;
    return -1;
  }
  size_t nWritten = fwrite( data, sizeof( double ), n, f );
  fclose( f );
  std::cout << nWritten << " échantillons écrits dans " << filename << std::endl;
  return 0;
}

int inout( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
           double streamTime, RtAudioStreamStatus status, void *data )
{
  // Since the number of input and output channels is equal, we can do
  // a simple buffer copy operation here.
  if ( status ) std::cout << "Stream over/underflow detected." << std::endl;

  if ( streamTime >= streamTimePrintTime ) {
    std::cout << "streamTime = " << streamTime << std::endl;
    streamTimePrintTime += streamTimePrintIncrement;
  }

  UserData *ud     = (UserData *) data;
  MY_TYPE  *micIn  = (MY_TYPE *) inputBuffer;
  MY_TYPE  *outBuf = (MY_TYPE *) outputBuffer;
  (void) micIn;

  // 1. Remplissage du buffer d'entrée mono
  for ( unsigned int i = 0; i < nBufferFrames; i++ ) {
    if (USE_FILE){
      ud->in[i] = ud->fileData[ud->readIndex];
      ud->readIndex++;
      if ( ud->readIndex >= ud->fileLength ) ud->readIndex = 0;
    }
    else {
      ud->in[i] = micIn[i * ud->nChannels];
    }
  }

  // 2. Traitement
  for ( unsigned int i = 0; i < nBufferFrames; i++ )
    ud->out[i] = 0.5*ud->in[i];

  // 3. Envoi vers la carte son
  for ( unsigned int i = 0; i < nBufferFrames; i++ )
    for ( unsigned int c = 0; c < ud->nChannels; c++ )
      outBuf[i * ud->nChannels + c] = ud->out[i];

  // 4. Enresigtrement du dump in et dump out
  write_buff_dump( ud->in, nBufferFrames, ud->dumpIn, ud->nDump, &ud->indDumpIn );
  write_buff_dump( ud->out, nBufferFrames, ud->dumpOut, ud->nDump, &ud->indDumpOut );
  return 0;
}

int main( int argc, char *argv[] )
{
  unsigned int channels, fs, oDevice = 0, iDevice = 0, iOffset = 0, oOffset = 0;
  UserData userData;

  // Minimal command-line checking
  if (argc < 3 || argc > 7 ) usage();

  userData.fileData = loadBinaryFile( FILENAME, &userData.fileLength );
  if ( userData.fileData == NULL ) exit( 1 );
  userData.readIndex = 0;
  std::cout << "Fichier chargé : " << userData.fileLength << " échantillons" << std::endl;

  RtAudio adac;
  std::vector<unsigned int> deviceIds = adac.getDeviceIds();
  if ( deviceIds.size() < 1 ) {
    std::cout << "\nNo audio devices found!\n";
    exit( 1 );
  }

  channels = (unsigned int) atoi(argv[1]);
  fs = (unsigned int) atoi(argv[2]);
  if ( argc > 3 )
    iDevice = (unsigned int) atoi(argv[3]);
  if ( argc > 4 )
    oDevice = (unsigned int) atoi(argv[4]);
  if ( argc > 5 )
    iOffset = (unsigned int) atoi(argv[5]);
  if ( argc > 6 )
    oOffset = (unsigned int) atoi(argv[6]);

  userData.nDump      = DUMP_SECONDS * fs;
  userData.dumpIn     = (double *) calloc( userData.nDump, sizeof( double ) );
  userData.dumpOut    = (double *) calloc( userData.nDump, sizeof( double ) );
  userData.indDumpIn  = 0;
  userData.indDumpOut = 0;
  if ( userData.dumpIn == NULL || userData.dumpOut == NULL ) exit( 1 );

  // Let RtAudio print messages to stderr.
  adac.showWarnings( true );

  // Set the same number of channels for both input and output.
  unsigned int bufferFrames = 512;
  RtAudio::StreamParameters iParams, oParams;
  iParams.nChannels = channels;
  iParams.firstChannel = iOffset;
  oParams.nChannels = channels;
  oParams.firstChannel = oOffset;

  if ( iDevice == 0 )
    iParams.deviceId = adac.getDefaultInputDevice();
  else {
    if ( iDevice >= deviceIds.size() )
      iDevice = getDeviceIndex( adac.getDeviceNames(), true );
    iParams.deviceId = deviceIds[iDevice];
  }
  if ( oDevice == 0 )
    oParams.deviceId = adac.getDefaultOutputDevice();
  else {
    if ( oDevice >= deviceIds.size() )
      oDevice = getDeviceIndex( adac.getDeviceNames() );
    oParams.deviceId = deviceIds[oDevice];
  }
  
  RtAudio::StreamOptions options;
  //options.flags |= RTAUDIO_NONINTERLEAVED;

  userData.nChannels    = channels;
  userData.fs           = fs;
  userData.bufferFrames = bufferFrames;
  userData.bufferBytes  = bufferFrames * channels * sizeof( MY_TYPE );
  if ( adac.openStream( &oParams, &iParams, FORMAT, fs, &bufferFrames, &inout, (void *)&userData, &options ) ) {
    goto cleanup;
  }

  if ( adac.isStreamOpen() == false ) goto cleanup;

  // openStream() peut avoir modifié bufferFrames : mise à jour de la structure
  userData.bufferFrames = bufferFrames;
  userData.bufferBytes  = bufferFrames * channels * sizeof( MY_TYPE );

  userData.in  = (double *) calloc( bufferFrames, sizeof( double ) );
  userData.out = (double *) calloc( bufferFrames, sizeof( double ) );
  if ( userData.in == NULL || userData.out == NULL ) goto cleanup;

  // Test RtAudio functionality for reporting latency.
  std::cout << "\nStream latency = " << adac.getStreamLatency() << " frames" << std::endl;

  if ( adac.startStream() ) goto cleanup;

  char input;
  std::cout << "\nRunning ... press <enter> to quit (buffer frames = " << bufferFrames << ").\n";
  std::cin.get(input);

  // Stop the stream.
  if ( adac.isStreamRunning() )
    adac.stopStream();

  writeBinaryFile( DUMP_IN_FILENAME, userData.dumpIn, userData.indDumpIn );
  writeBinaryFile( DUMP_OUT_FILENAME, userData.dumpOut, userData.indDumpOut );

cleanup:
  if ( adac.isStreamOpen() ) adac.closeStream();

  free( userData.fileData );
  free( userData.in );
  free( userData.out );
  free( userData.dumpIn );
  free( userData.dumpOut );

  return 0;
}
