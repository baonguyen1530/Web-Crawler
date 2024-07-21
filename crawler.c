#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <signal.h>
#include <stdbool.h>

#define _POSIX_C_SOURCE 200809L //Defines POSIX version

// Define a structure for queue elements.
typedef struct URLQueueNode {
    char *url;
    int depth;      // This line represents the depth parameter
    struct URLQueueNode *next;
} URLQueueNode;

// Define a structure for a thread-safe queue.
typedef struct {
    URLQueueNode *head, *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond; // Condition variable
} URLQueue;

// Global variables for handling interrupts
volatile sig_atomic_t pending_interrupt = 0;
pthread_mutex_t interrupt_lock;

// Global variable to indicate if the maximum depth has been reached
volatile bool max_depth_reached = false;

// Global variable for maximum depth
int MAX_DEPTH; 

//This function writes an error message to a file named "error_log.txt". 
void write_error_to_file(const char *error_message) {
    FILE *error_file = fopen("error_log.txt", "a");  //open error log file for append
    if (error_file != NULL) {
        fprintf(error_file, "%s\n", error_message);
        fclose(error_file);
    } else {
        fprintf(stderr, "Error: Unable to open error log file\n");
    }
}

// Initialize a URL queue.
void initQueue(URLQueue *queue) {
    queue->head = queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL); // Initialize condition variable
}

// Add a URL to the queue.
void enqueue(URLQueue *queue, const char *url, int depth) {
    URLQueueNode *newNode = malloc(sizeof(URLQueueNode));
    
    // Copy the URL string and assign it to the new node
    newNode->url = strdup(url);
    
    // Set the depth of the URL in the new node
    newNode->depth = depth;
    
    // Initialize the 'next' pointer of the new node to NULL
    newNode->next = NULL;

    pthread_mutex_lock(&queue->lock);
    if (queue->tail) {
        // If the queue is not empty, set the 'next' pointer of the current tail to the new node
        queue->tail->next = newNode;
    } else {
        // If the queue is empty, set both head and tail pointers to the new node
        queue->head = newNode;
    }
    
    // Update the tail pointer to point to the new node
    queue->tail = newNode;
    pthread_mutex_unlock(&queue->lock);
}

// This function removes a URL from the front of the queue and returns it along with its depth.
char *dequeue(URLQueue *queue, int *depth) {
    pthread_mutex_lock(&queue->lock);
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    URLQueueNode *temp = queue->head; 
    char *url = temp->url; // Get the URL from the node at the head of the queue
    *depth = temp->depth; // Retrieve the depth of the URL
    queue->head = queue->head->next; 
    if (queue->head == NULL) {
        queue->tail = NULL; 
    }
    free(temp);
    pthread_mutex_unlock(&queue->lock);
    return url;
}

// Struct to hold the response data
struct MemoryStruct {
    char *memory;
    size_t size;
};

// This is a callback function used by libcurl to handle fetched data. It appends the fetched data to a buffer allocated in the struct MemoryStruct.
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb; // Calculate the total size of the data received
    struct MemoryStruct *mem = (struct MemoryStruct *)userp; // Cast the user data pointer to a struct MemoryStruct pointer

    // Allocate memory for the response
    char *ptr_mem = realloc(mem->memory, mem->size + realsize + 1); // Reallocate memory for the response buffer
    if (ptr_mem == NULL) { 
        // Out of memory!
        fprintf(stderr, "Out of memory!\n"); 
        return 0; 
    }

    mem->memory = ptr_mem; // Update the memory pointer in the MemoryStruct
    memcpy(&(mem->memory[mem->size]), ptr, realsize); 
    mem->size += realsize; 
    mem->memory[mem->size] = 0; 

    return realsize; 
}

//  This function fetches the content of a URL using libcurl. 
char *perform_fetch(const char *url) {
    CURL *curl_handle; 
    CURLcode res; 
    struct MemoryStruct chunk; // Declare a struct to hold the fetched data

    chunk.memory = malloc(1);  // Allocate memory for the fetched data buffer, start with an empty buffer
    chunk.size = 0;

    // Initialize CURL handle
    curl_handle = curl_easy_init(); // Initialize the CURL handle
    if (!curl_handle) { 
        fprintf(stderr, "Error: Failed to initialize CURL\n"); 
	    free(chunk.memory); 
        return NULL; 
    }

    // Set the URL to fetch
    curl_easy_setopt(curl_handle, CURLOPT_URL, url); 

    // Set the write callback function
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback); 

    // Pass our 'chunk' struct to the callback function
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk); 

    // Perform the request
    res = curl_easy_perform(curl_handle); 

    // Check for errors
    if (res != CURLE_OK) { 
        char error_message[100]; 
        snprintf(error_message, sizeof(error_message), "CURL Error: %s", curl_easy_strerror(res)); 
        write_error_to_file(error_message);
        curl_easy_cleanup(curl_handle); 
        free(chunk.memory); 
        return NULL; 
    }

    // Cleanup
    curl_easy_cleanup(curl_handle); 

    // Null-terminate the buffer
    chunk.memory[chunk.size] = '\0'; 

    // Return the fetched data
    return chunk.memory; 
}

// This function extracts links from HTML content and enqueues them with their depths. 
void extract_links(const char *html_content, URLQueue *queue, int depth) {
    const char *start = html_content; 
    const char *tag = "<a href=\"";
    const char *end_tag = "\">"; 
    const size_t tag_len = strlen(tag); 
    const size_t end_tag_len = strlen(end_tag);
    const char *valid_protocols[] = {"http://","https://"}; // Define an array of valid protocols
    const int num_protocols = sizeof(valid_protocols)/ sizeof(valid_protocols[0]); 

    // Loop through the HTML content until no more links are found
    while ((start = strstr(start, tag)) != NULL) {
        start += tag_len; 
        const char *end = strstr(start, end_tag);
        if (end == NULL)
            break;

        size_t url_len = end - start; 
        char *url = malloc(url_len + 1); 
        if (url == NULL) { 
            fprintf(stderr, "Error: Memory allocation failed for URL\n");
            break;
        }

        strncpy(url, start, url_len); 
        url[url_len] = '\0'; 

        // Check if the URL starts with a valid protocol
        bool valid_url = false;
        for (int i = 0; i < num_protocols; i++) {
            if (strncmp(url, valid_protocols[i], strlen(valid_protocols[i])) == 0) {
                valid_url = true; // Set the flag to indicate a valid URL
                break;
            }
        }

        // If the URL is valid, enqueue it with its depth
        if (valid_url) {
            enqueue(queue, url, depth); 
        } else {  
            char error_message[100]; 
            snprintf(error_message, sizeof(error_message), "Invalid URL format: %s", url);
            write_error_to_file(error_message); // Write error message to file
            free(url); 
        }

        start = end + end_tag_len; 
    }
}

// Function to fetch a URL and extract links
void *fetch_url(void *arg) {
    URLQueue *queue = (URLQueue *)arg;
    char *url; 

    pthread_mutex_lock(&queue->lock);
    while (!pending_interrupt && !max_depth_reached) {
        pthread_mutex_unlock(&queue->lock);

        int depth; // Declare a variable to hold the depth of the URL
        url = dequeue(queue, &depth); // Dequeue a URL from the queue along with its depth
        if (url == NULL) 
            break;

        if (depth >= MAX_DEPTH) { 
            pthread_mutex_lock(&interrupt_lock); // Lock access to the interrupt flag
            max_depth_reached = true; 
            pthread_mutex_unlock(&interrupt_lock); // Unlock access to the interrupt flag
            pthread_cond_broadcast(&queue->cond); // Broadcast the condition to wake up other threads
            free(url); 
            break; 
        }

        char *response = perform_fetch(url); // Fetch the URL and store the response
        if (response != NULL) { 
            extract_links(response, queue, depth + 1); 
            printf("Fetched URL[%d]: %s\n", depth, url); 
            free(response); 
        }
        free(url); 

        pthread_mutex_lock(&queue->lock);
    }
    pthread_mutex_unlock(&queue->lock);

    // Signal other threads to wake up
    pthread_cond_broadcast(&queue->cond); 

    return NULL; 
}

//This is a signal handler function for interrupt signals (SIGINT). It sets a flag indicating a pending interrupt.
void sighandler(int signum) {
    pthread_mutex_lock(&interrupt_lock); 
    pending_interrupt = 1; 
    pthread_mutex_unlock(&interrupt_lock); 
}

// Main function to drive the web crawler.
int main(int argc, char *argv[]) {
    if (argc < 3) { // Check if the command-line arguments are less than 3
        printf("Usage: %s <starting-url> <max-depth>\n", argv[0]); // Print usage instructions
        return 1; 
    }

    // Initialize interrupt handling
    pthread_mutex_init(&interrupt_lock, NULL);
    signal(SIGINT, sighandler); 

    URLQueue queue; 
    initQueue(&queue); 
    enqueue(&queue, argv[1], 0); 

    const int NUM_THREADS = 4; 
    pthread_t threads[NUM_THREADS]; 

    MAX_DEPTH = atoi(argv[2]); 
    
    // Check if the MAX_DEPTH is negative
    if (MAX_DEPTH < 0) {
        printf("Error: Maximum depth cannot be negative.\n");
        return 1;
    }

    while (1) { 
        // Create threads to fetch URLs
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, fetch_url, &queue);
        }

        // Wait for threads to finish or condition variable signal
        pthread_mutex_lock(&queue.lock); 
        pthread_cond_wait(&queue.cond, &queue.lock); 
        pthread_mutex_unlock(&queue.lock); 

        pthread_mutex_lock(&interrupt_lock);
        if (pending_interrupt || max_depth_reached) { 
            pthread_mutex_unlock(&interrupt_lock); 
            break;
        }
        pthread_mutex_unlock(&interrupt_lock); 
    }

    // Cleanup logic
    URLQueueNode *current = queue.head; 
    URLQueueNode *next; // Declare a pointer to hold the next node while freeing the current one
    while (current != NULL) { 
        next = current->next; 
        free(current->url); 
        free(current); 
        current = next; 
    }

    pthread_mutex_destroy(&interrupt_lock); // Destroy the interrupt lock
    pthread_cond_destroy(&queue.cond); // Destroy the condition variable associated with the queue

    return 0; 
}
