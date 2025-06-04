#define _POSIX_C_SOURCE 200809L
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<signal.h>

#ifdef __OpenBSD__
#define SIGPLUS             SIGUSR1+1
#define SIGMINUS            SIGUSR1-1
#else
#define SIGPLUS             SIGRTMIN
#define SIGMINUS            SIGRTMIN
#endif

#define LENGTH(X)           (sizeof(X) / sizeof (X[0]))
#define CMDLENGTH           50
#define MIN( a, b ) ( ( a < b) ? a : b )
#define STATUSLENGTH (LENGTH(blocks) * CMDLENGTH + 1)

typedef struct {
    char* icon;
    char* command;
    unsigned int interval;
    unsigned int signal;
} Block;

// Forward declarations for functions that will be defined later.
// These are necessary because some functions call others before they are defined.
#ifndef __OpenBSD__
void dummysighandler(int num);
#endif
void sighandler(int num);
void getcmds(int time);
void getsigcmds(unsigned int signal);
void setupsignals();
// Note: sighandler is declared twice, but this is harmless.
// The crucial part is its definition later.
// void sighandler(int signum); // Already declared above
int getstatus(char *str, char *last);
void statusloop();
void termhandler(int signum); // Corrected signature: accepts int argument
void sigpipehandler(int signum); // Corrected signature: accepts int argument
void pstdout();
void psomebar();
static void (*writestatus) () = psomebar;

// Include the blocks.h file which defines the 'blocks' array and 'delim'/'delimLen'
#include "blocks.h" // This file is expected to define 'blocks', 'delim', and 'delimLen'

static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[2][STATUSLENGTH];
static int statusContinue = 1;
static int returnStatus = 0; // This variable seems unused in the provided code.
static char somebarPath[128];
static int somebarFd = -1;

/**
 * @brief Executes a command and stores its output, prefixed with an icon.
 * @param block Pointer to the Block structure containing command and icon.
 * @param output Buffer to store the command's output.
 */
void getcmd(const Block *block, char *output)
{
    // Copy the icon to the output buffer
    strcpy(output, block->icon);
    // Open a pipe to execute the command
    FILE *cmdf = popen(block->command, "r");
    if (!cmdf) {
        // If popen fails, print an error and return.
        perror("popen failed");
        return;
    }
    // Get the current length of the output (after copying icon)
    int i = strlen(block->icon);
    // Read the command's output into the buffer, after the icon
    // Ensure we don't overflow the buffer, leaving space for delimiter and null terminator.
    if (fgets(output + i, CMDLENGTH - i - delimLen, cmdf) == NULL) {
        // If fgets fails or reads nothing, handle it.
        // The buffer might contain just the icon.
    }

    // Get the new length of the output string
    i = strlen(output);
    if (i == 0) {
        // If block icon and command output are both empty, close the pipe and return.
        pclose(cmdf);
        return;
    }

    // If a delimiter is defined, append it
    if (delim[0] != '\0') {
        // Only chop off newline if one is present at the end of the command's output
        // The `i` variable now points to the last character or one after the last.
        // If the last character read by fgets was a newline, overwrite it.
        if (i > 0 && output[i - 1] == '\n') {
            output[i - 1] = '\0'; // Remove the newline
            i--; // Adjust length
        }
        // Append the delimiter
        strncpy(output + i, delim, delimLen);
        output[i + delimLen] = '\0'; // Ensure null termination after delimiter
    } else {
        // If no delimiter, just ensure null termination, removing any trailing newline.
        if (i > 0 && output[i - 1] == '\n') {
            output[i - 1] = '\0';
        } else {
            output[i] = '\0'; // Just null terminate if no newline
        }
    }
    pclose(cmdf); // Close the pipe
}

/**
 * @brief Updates status bar blocks based on their interval.
 * @param time Current time counter. If -1, update all blocks.
 */
void getcmds(int time)
{
    const Block* current;
    for (unsigned int i = 0; i < LENGTH(blocks); i++) {
        current = blocks + i;
        // Update block if interval is 0 (always update), or if time is a multiple of interval,
        // or if time is -1 (initial update for all blocks).
        if ((current->interval != 0 && time % current->interval == 0) || time == -1)
            getcmd(current, statusbar[i]);
    }
}

/**
 * @brief Updates status bar blocks based on a received signal.
 * @param signal The signal number to check against block configurations.
 */
void getsigcmds(unsigned int signal)
{
    const Block *current;
    for (unsigned int i = 0; i < LENGTH(blocks); i++) {
        current = blocks + i;
        if (current->signal == signal)
            getcmd(current, statusbar[i]);
    }
}

/**
 * @brief Sets up signal handlers for real-time signals and custom block signals.
 */
void setupsignals()
{
    struct sigaction sa = {0};
#ifndef __OpenBSD__
    /* initialize all real time signals with dummy handler */
    sa.sa_handler = dummysighandler;
    for (int i = SIGRTMIN; i <= SIGRTMAX; i++)
        sigaction(i, &sa, NULL);
#endif

    // Set up handler for custom block signals
    sa.sa_handler = sighandler;
    for (unsigned int i = 0; i < LENGTH(blocks); i++) {
        if (blocks[i].signal > 0)
            // Register signal handler for SIGMINUS + block's signal number
            sigaction(SIGMINUS + blocks[i].signal, &sa, NULL);
    }
}

/**
 * @brief Concatenates all status bar block strings into a single status string.
 * @param str The buffer to store the new concatenated status string.
 * @param last The buffer holding the previous status string for comparison.
 * @return 0 if the new status string is the same as the last one, non-zero otherwise.
 */
int getstatus(char *str, char *last)
{
    strcpy(last, str); // Save the current status string as 'last'
    str[0] = '\0';      // Clear the 'str' buffer
    // Concatenate all block status strings
    for (unsigned int i = 0; i < LENGTH(blocks); i++)
        strcat(str, statusbar[i]);

    // Remove the trailing delimiter from the concatenated string
    // This assumes the last block always adds a delimiter.
    // Ensure that str is long enough and contains at least delimLen characters.
    if (strlen(str) >= strlen(delim)) {
        str[strlen(str) - strlen(delim)] = '\0';
    }

    return strcmp(str, last); // Return 0 if strings are identical, non-zero otherwise
}

/**
 * @brief Writes the status string to standard output.
 * Only writes if the status string has changed.
 */
void pstdout()
{
    if (!getstatus(statusstr[0], statusstr[1])) // Only write out if text has changed.
        return;
    printf("%s\n", statusstr[0]);
    fflush(stdout); // Ensure the output is immediately written
}

/**
 * @brief Writes the status string to the somebar FIFO/socket.
 * Only writes if the status string has changed. Handles opening the FIFO.
 */
void psomebar()
{
    if (!getstatus(statusstr[0], statusstr[1])) // Only write out if text has changed.
        return;
    if (somebarFd < 0) {
        // Try to open the somebar path for writing
        somebarFd = open(somebarPath, O_WRONLY | O_CLOEXEC);
        if (somebarFd < 0 && errno == ENOENT) {
            // If file not found, assume somebar is not ready yet, wait and retry.
            // This is a common pattern for FIFOs that might not exist yet.
            sleep(1);
            somebarFd = open(somebarPath, O_WRONLY | O_CLOEXEC);
        }
        if (somebarFd < 0) {
            // If opening still fails, print an error and return.
            perror("open somebarPath");
            return;
        }
    }
    // Write the status command and string to the somebar file descriptor
    dprintf(somebarFd, "status %s\n", statusstr[0]);
}

/**
 * @brief Main loop for updating and writing the status bar.
 * Continuously updates blocks, writes status, and sleeps.
 */
void statusloop()
{
    setupsignals(); // Set up signal handlers
    int i = 0;
    getcmds(-1); // Initial update for all blocks (time = -1)
    while (statusContinue) { // Loop continues as long as statusContinue is true
        getcmds(i++);       // Update blocks based on interval
        writestatus();      // Write the current status
        // If statusContinue is set to 0 by a signal handler, break the loop
        if (!statusContinue)
            break;
        sleep(1); // Wait for 1 second before next update
    }
}

#ifndef __OpenBSD__
/*
 * @brief Dummy signal handler.
 * This handler does nothing and is used for real-time signals on non-OpenBSD systems.
 * @param signum The signal number.
 */
void dummysighandler(int signum)
{
    // (void)signum; // Suppress unused parameter warning if not using signum
    return;
}
#endif

/**
 * @brief Generic signal handler for custom block signals.
 * @param signum The signal number that triggered the handler.
 */
void sighandler(int signum)
{
    getsigcmds(signum - SIGPLUS); // Update blocks associated with this signal
    writestatus(); // Write the updated status
}

/**
 * @brief Signal handler for termination signals (SIGTERM, SIGINT).
 * Sets a flag to stop the main status loop gracefully.
 * @param signum The signal number (e.g., SIGTERM, SIGINT).
 */
void termhandler(int signum)
{
    (void)signum; // Suppress unused parameter warning
    statusContinue = 0; // Set flag to exit the statusloop
}

/**
 * @brief Signal handler for SIGPIPE.
 * Closes and resets the somebar file descriptor, typically when the reading end closes.
 * @param signum The signal number (SIGPIPE).
 */
void sigpipehandler(int signum)
{
    (void)signum; // Suppress unused parameter warning
    close(somebarFd); // Close the broken pipe/socket
    somebarFd = -1;   // Reset the file descriptor
}

/**
 * @brief Main function of the someblocks program.
 * Parses command-line arguments, sets up paths, registers signal handlers,
 * and starts the status update loop.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return Exit status of the program.
 */
int main(int argc, char** argv)
{
    // Parse command line arguments
    for (int i = 0; i < argc; i++) {
        if (!strcmp("-d", argv[i])) {
            // Set delimiter
            strncpy(delim, argv[++i], delimLen);
        } else if (!strcmp("-p", argv[i])) {
            // Set output to stdout
            writestatus = pstdout;
        } else if (!strcmp("-s", argv[i])) {
            // Set somebar path
            strcpy(somebarPath, argv[++i]);
        }
    }

    // If somebarPath is not set via command line, use XDG_RUNTIME_DIR
    if (!strlen(somebarPath)) {
        strcpy(somebarPath, getenv("XDG_RUNTIME_DIR"));
        strcat(somebarPath, "/somebar-0");
    }

    // Adjust delimiter length and ensure null termination
    delimLen = MIN(delimLen, strlen(delim));
    delim[delimLen++] = '\0'; // Increment delimLen to account for the null terminator

    // Register termination and pipe signal handlers
    // These calls are now correct because termhandler and sigpipehandler
    // accept an int argument.
    signal(SIGTERM, termhandler);
    signal(SIGINT, termhandler);
    signal(SIGPIPE, sigpipehandler);

    statusloop(); // Start the main status update loop
    return 0;     // Program exits when statusloop breaks
}

