// AudioDLL.cpp : Defines the exported functions for the DLL application.
// Code for block handling and rendering adapted from https://www.planet-source-code.com/vb/scripts/ShowCode.asp?txtCodeId=4422&lngWId=3

#include "pch.h"
#include "AudioDLL.h"

/* Default size and count for an audio block */
#define BLOCK_SIZE 8192
#define BLOCK_COUNT 20

/* Function prototypes */
static void CALLBACK WaveOutProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);
static WAVEHDR* AllocateBlocks(int size, int count);
static void FreeBlocks(WAVEHDR* blockArray);
static void WriteAudio(HWAVEOUT hWaveOut, LPSTR data, int size, int blockSize, int blockCount);

/* Module level variables */
static CRITICAL_SECTION waveCriticalSection;
static WAVEHDR* waveBlocks;
static volatile int waveFreeBlockCount;
static int waveCurrentBlock;

/* Struct to hold header data from wav file */
struct wav_header
{
    /* RIFF Chunk */
    char cbChunkID[4]; // RIFF
    unsigned long ulChunkSize;
    char cbFormat[4]; // WAVE
    /* Format Chunk */
    char cbSubchunk1ID[4]; // fmt
    unsigned long ulSubchunk1Size;
    unsigned short wAudioFormatCode;
    unsigned short wNumChannels;
    unsigned long ulSampleRate; // Samples per second
    unsigned long ulByteRate; // Num bytes required for one second of audio data
    unsigned short wBlockAlign; // Bytes required to store single sample frame
    unsigned short wBitsPerSample;
};

/* Struct to hold chunk from wav file */
struct chunk {
    char cbChunkID[4];
    unsigned long ulChunkSize;
};

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: WaveOutProc

  Summary:  Callback function for waveOutOpen. Called when the device is
            opened, closed, or when a block is finished. This waits for
            a block to be finished writing indicated by a WOM_DONE message
            then increments waveFreeBlockCount so that a new block can be
            written.

  Args:     HWAVEOUT hWaveOut
              Handle to callback associated waveform-audio device.
            UINT uMsg
              Waveform-audio output message,
            DWORD dwInstance
              User-instance data specified with waveOutOpen(waveFreeBlockCount).
            DWORD dwParam1
              Message parameter.
            DWORD dwParam2
              Message parameter.
-----------------------------------------------------------------F-F*/
static void CALLBACK WaveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance,
    DWORD dwParam1, DWORD dwParam2) {
    if (uMsg != WOM_DONE) {
        return;
    }
    EnterCriticalSection(&waveCriticalSection);
    waveFreeBlockCount++;
    LeaveCriticalSection(&waveCriticalSection);
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: AllocateBlocks

  Summary:  Allocates the necessary blocks in which the raw audio data
            will be written to. First gets a pointer to char buffer which
            is returned from HeapAlloc after a cast. Casts the buffer to
            a pointer to WAVEHDR in which the buffer allocates the blocks
            with the wave headers.

  Args:     int size
              The size of the block.
            int count
              The number of blocks.

  Returns:  WAVEHDR*
              The array of blocks of audio to be written to with the
              wave header.
-----------------------------------------------------------------F-F*/
WAVEHDR* AllocateBlocks(int size, int count) {
    unsigned char* buffer;
    int i;
    WAVEHDR* blocks;
    DWORD dwTotalBufferSize = (size + sizeof(WAVEHDR)) * count;

    // Allocate memory
    if ((buffer = reinterpret_cast<unsigned char*>(HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        dwTotalBufferSize
    ))) == NULL) {
        ExitProcess(1);
    }

    blocks = (WAVEHDR*)buffer;
    buffer += sizeof(WAVEHDR) * count;
    for (int i = 0; i < count; i++) {
        blocks[i].dwBufferLength = size;
        blocks[i].lpData = (LPSTR)buffer;
        buffer += size;
    }

    return blocks;
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: FreeBlocks

  Summary:  Frees the allocated blocks from memory by calling HeapFree
            to free the blocks on the heap allocated in AllocateBlocks.

  Args:     WAVEHDR* blockArray
              Array of the audio blocks.
-----------------------------------------------------------------F-F*/
void FreeBlocks(WAVEHDR* blockArray) {
    HeapFree(GetProcessHeap(), 0, blockArray);
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: WriteAudio

  Summary:  Writes audio to a free audio block. Checks to see if there is
            data to write and if there is it grabs the current block checks
            to see if it is prepared and unprepares it if so. Once unprepared,
            it writes as much data it can to then exits. If a block was unprepared,
            the function writes as much data as possible to fill the block, prepares
            it then writes it to the waveform device in addition to decrementing the
            free block counter. Before returning, the function gets how much is left
            to write then waits until a block is free until finally updating the block
            pointer to the next free block.

  Args:     HWAVEOUT hWaveOut
              Handle to waveform-audio output device.
            LPSTR data
              Buffer data to be written to the audio block.
            int size
              Size of the buffer.
            int blockSize
              Size of the audio block.
            int blockCount
              The amount of blocks.
-----------------------------------------------------------------F-F*/
void WriteAudio(HWAVEOUT hWaveOut, LPSTR data, int size, int blockSize, int blockCount) {
    WAVEHDR* current;
    int remain;
    current = &waveBlocks[waveCurrentBlock];
    while (size > 0) {
        if (current->dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));
        }

        if (size < (int)(blockSize - current->dwUser)) {
            memcpy(current->lpData + current->dwUser, data, size);
            current->dwUser += size;
            break;
        }

        remain = blockSize - current->dwUser;
        memcpy(current->lpData + current->dwUser, data, remain);
        size -= remain;
        data += remain;
        current->dwBufferLength = blockSize;
        waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));
        EnterCriticalSection(&waveCriticalSection);
        waveFreeBlockCount--;
        LeaveCriticalSection(&waveCriticalSection);


        while (!waveFreeBlockCount) {
            Sleep(10);
        }

        waveCurrentBlock++;
        waveCurrentBlock %= blockCount;
        current = &waveBlocks[waveCurrentBlock];
        current->dwUser = 0;
    }
}

/*F+F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F
  Function: PlayFrame

  Summary:  This function plays a single frame of the wav audio. Using the
            parameters passed from the Lua FFI call this first
            finds the calculates how many bytes represents a single frame of
            the ITMF the determines the number of bytes to seek to the frame
            that the ITMF is at. Once the file is at the position of the frame,
            this creates a buffer 1/8th of the side to reduce latency which is
            written into an audio block the size of a single frame before being
            played back. Before the function exits the necessary clean-up is
            completed.

  Args:     const char* fileName
              Path to the wav file that is going to be played. This is specified
              from the Lua FFI call.
            double frameTime
              The length of a single frame in seconds. Specified from Lua FFI call.
            double timeFrom
                Time in which the frame starts at in seconds. Specified from Lua FFI
                call.
F---F---F---F---F---F---F---F---F---F---F---F---F---F---F---F---F-F*/
void PlayFrame(const char* fileName, double frameTime, double timeFrom)
{
    wav_header waveHeader;
    HWAVEOUT hWaveOut;
    HANDLE hFile;
    WAVEFORMATEX wfx;
    MMRESULT result;
    int i;

    // Convert char* to wchar_t* for passing into CreateFile
    int iNumChars = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, NULL, 0);
    wchar_t* wcpFileName = new wchar_t[iNumChars];
    MultiByteToWideChar(CP_UTF8, 0, fileName, -1, wcpFileName, iNumChars);

    // Create file from filename
    if ((hFile = CreateFile(
        wcpFileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    )) == INVALID_HANDLE_VALUE) {
        ExitProcess(1);
    }

    // Read the header
    DWORD dwHeaderBytesRead;
    ReadFile(hFile, &waveHeader, sizeof(waveHeader), &dwHeaderBytesRead, NULL);


    if (waveHeader.wAudioFormatCode != 1) {
        // If the audio format code isn't 1 for WAVE_FORMAT_PCM exit
        ExitProcess(1);
    }

    chunk c;
    while (true) {
        DWORD dwChunkBytesRead;
        ReadFile(hFile, &c, sizeof(c), &dwChunkBytesRead, NULL);
        if (*(unsigned int*)&c.cbChunkID == 0x61746164) {
            break;
        }
        SetFilePointer(hFile, c.ulChunkSize, NULL, FILE_CURRENT);
    }
    // File pointer will now be at the data chunk

    // Calculate the size for a block - this will be the number of bytes needed for a single
    // frame in ITMF
    // This is found by ByteRate * frameTime
    int iSingleFrameByteRate = waveHeader.ulByteRate * frameTime;

    // Find the bytes to seek to location in file where the timeFrom is ByteRate * timeFrom
    long lBytesToTimeFrom = waveHeader.ulByteRate * timeFrom;
    int blockCount = 1;
    // Allocate the blocks for the wave file
    waveBlocks = AllocateBlocks(iSingleFrameByteRate, blockCount);
    waveFreeBlockCount = blockCount;
    waveCurrentBlock = 0;
    InitializeCriticalSection(&waveCriticalSection);

    // Set up wfx to match input wav file
    wfx.nSamplesPerSec = waveHeader.ulSampleRate;
    wfx.wBitsPerSample = waveHeader.wBitsPerSample;
    wfx.nChannels = waveHeader.wNumChannels;
    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels) >> 3;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    if (waveOutOpen(
        &hWaveOut,
        WAVE_MAPPER,
        &wfx,
        (DWORD_PTR)WaveOutProc,
        (DWORD_PTR)&waveFreeBlockCount,
        CALLBACK_FUNCTION | WAVE_ALLOWSYNC
    ) != MMSYSERR_NOERROR) {
        ExitProcess(1);
    }

    // Seek to location in file of timeFrom
    SetFilePointer(hFile, lBytesToTimeFrom, NULL, FILE_CURRENT);

    int bufferSize = iSingleFrameByteRate / 8;
    char* buffer = new char[bufferSize];
    // Playback
    DWORD dwTotalReadBytes = 0;

    while (true) {
        DWORD dwReadBytes;

        if (!ReadFile(hFile, buffer, bufferSize, &dwReadBytes, NULL)) {
            break;
        }

        if (dwReadBytes == 0) {
            break;
        }

        if (dwReadBytes < bufferSize) {
            memset(buffer + dwReadBytes, 0, bufferSize - dwReadBytes);
        }

        WriteAudio(hWaveOut, buffer, bufferSize, iSingleFrameByteRate, blockCount);
        dwTotalReadBytes += dwReadBytes;


        if (dwTotalReadBytes == iSingleFrameByteRate) {
            // check to see if the read bytes is equal to a frame time
            break;
        }

    }

    while (waveFreeBlockCount < blockCount) {
        Sleep(10);
    }

    for (int i = 0; i < waveFreeBlockCount; i++) {
        if (waveBlocks[i].dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR));
        }
    }

    DeleteCriticalSection(&waveCriticalSection);
    FreeBlocks(waveBlocks);
    waveOutClose(hWaveOut);
    CloseHandle(hFile);
    delete[] wcpFileName;
    delete[] buffer;

    return;
}

/*F+F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F
  Function: PlayWavFile

  Summary:  This function plays an entire wav file back synchronously. This is
            behaves similarly to in which the wav file is opened and the audio data
            is written into a specified sized block determined by BLOCK_SIZE. This
            writes the audio block by block for BLOCK_COUNT blocks ending when the
            wav file has finished playing. The necessary clean-up is then performed.

  Args:     const char* fileName
              Path to the wav file that is going to be played. This is specified
              from the Lua FFI call.
F---F---F---F---F---F---F---F---F---F---F---F---F---F---F---F---F-F*/
void PlayWavFile(const char* fileName)
{
    wav_header waveHeader;
    HWAVEOUT hWaveOut;
    HANDLE hFile;
    WAVEFORMATEX wfx;
    MMRESULT result;
    int i;
    char buffer[1024];

    // Convert char* to wchar_t* for passing into CreateFile(
    int iNumChars = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, NULL, 0);
    wchar_t* wcpFileName = new wchar_t[iNumChars];
    MultiByteToWideChar(CP_UTF8, 0, fileName, -1, wcpFileName, iNumChars);

    // Create file from filename
    if ((hFile = CreateFile(
        wcpFileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    )) == INVALID_HANDLE_VALUE) {
        ExitProcess(1);
    }

    // Read the header
    DWORD dwHeaderBytesRead;
    ReadFile(hFile, &waveHeader, sizeof(waveHeader), &dwHeaderBytesRead, NULL);

    // Allocate the blocks for the wave file
    waveBlocks = AllocateBlocks(BLOCK_SIZE, BLOCK_COUNT);
    waveFreeBlockCount = BLOCK_COUNT;
    waveCurrentBlock = 0;
    InitializeCriticalSection(&waveCriticalSection);

    // Set up wfx to match input wav file
    wfx.nSamplesPerSec = waveHeader.ulSampleRate;
    wfx.wBitsPerSample = waveHeader.wBitsPerSample;
    wfx.nChannels = waveHeader.wNumChannels;
    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels) >> 3;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    if (waveOutOpen(
        &hWaveOut,
        WAVE_MAPPER,
        &wfx,
        (DWORD_PTR)WaveOutProc,
        (DWORD_PTR)&waveFreeBlockCount,
        CALLBACK_FUNCTION
    ) != MMSYSERR_NOERROR) {
        ExitProcess(1);
    }

    // Playback
    while (true) {
        DWORD dwReadBytes;

        if (!ReadFile(hFile, buffer, sizeof(buffer), &dwReadBytes, NULL)) {
            break;
        }

        if (dwReadBytes == 0) {
            break;
        }

        if (dwReadBytes < sizeof(buffer)) {
            memset(buffer + dwReadBytes, 0, sizeof(buffer) - dwReadBytes);
        }

        WriteAudio(hWaveOut, buffer, sizeof(buffer), BLOCK_SIZE, BLOCK_COUNT);
    }

    while (waveFreeBlockCount < BLOCK_COUNT) {
        Sleep(10);
    }

    for (int i = 0; i < waveFreeBlockCount; i++) {
        if (waveBlocks[i].dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR));
        }
    }

    DeleteCriticalSection(&waveCriticalSection);
    FreeBlocks(waveBlocks);
    waveOutClose(hWaveOut);
    CloseHandle(hFile);
    delete[] wcpFileName;

    return;
}

/*F+F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F+++F
  Function: PlayWholeWavAsync

  Summary:  This function plays an entire wav file back asynchronously. Rather than
            using the audio buffers defined above, this uses the Windows PlaySound
            function with async flags. This provides an extremely simple implementation.

  Args:     const char* fileName
              Path to the wav file that is going to be played. This is specified
              from the Lua FFI call.
F---F---F---F---F---F---F---F---F---F---F---F---F---F---F---F---F-F*/
void PlayWholeWavAsync(const char* fileName) {
    if (fileName == NULL) {
        PlaySound(NULL, 0, 0); // Plays no sound

        return;
    }

    // Convert char* to wchar_t* for passing into PlaySound
    int iNumChars = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, NULL, 0);
    wchar_t* wcpFileName = new wchar_t[iNumChars];
    MultiByteToWideChar(CP_UTF8, 0, fileName, -1, wcpFileName, iNumChars);

    PlaySound(wcpFileName, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);

    return;
}