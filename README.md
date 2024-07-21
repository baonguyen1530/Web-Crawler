# Group Members
Bao Nguyen 211007991
Kevin Joy 210006152
Sammu Suryanarayanan 210006062

# Web Crawler

This is a simple web crawler implemented in C, designed to fetch web pages, extract links from them, and continue fetching pages from those links to a specified depth. The program utilizes multithreading to improve efficiency by fetching multiple URLs concurrently.

## Architecture
The architecture of the web crawler consists of several components:

- Main Function: The main function initializes the URL queue, sets up interrupt handling, and creates threads to fetch URLs. It also manages the termination condition based on either user interruption (SIGINT) or reaching the maximum depth.

- URL Queue: The URL queue is a thread-safe data structure that stores URLs along with their depths. It is implemented using a linked list structure with mutexes to ensure thread safety.

- Fetch URL Function: Each thread created by the main function runs the fetch_url function. This function dequeues URLs from the queue, fetches their content using libcurl, extracts links from the fetched HTML content, and enqueues them back into the queue for further processing.

- Signal Handler: A signal handler function (sighandler) is implemented to handle interrupt signals (SIGINT) gracefully. It sets a flag indicating a pending interrupt, allowing the program to terminate safely.

- Libcurl: The libcurl library is used to perform HTTP requests and fetch web page content. It provides functions to set up HTTP requests, handle responses, and manage connections.

## Multithreading Approach

The web crawler utilizes multithreading to improve performance by fetching multiple URLs concurrently. It creates a fixed number of worker threads, each responsible for dequeuing URLs from the queue, fetching their content, and enqueuing new URLs. The main thread coordinates the execution of worker threads and handles termination conditions.

## Libraries Used

- pthread: This library is used for creating and managing POSIX threads. It provides functions for thread creation, synchronization, and mutexes to ensure thread safety.

- libcurl: The libcurl library is utilized for performing HTTP requests and fetching web page content. It simplifies the process of sending HTTP requests, handling responses, and managing connections.

## Compilation

To compile the code, ensure you have `gcc` installed along with the necessary dependencies. Then, use the provided Makefile:
make

This will compile the code and generate an executable named `crawler`.

## Usage

To run the crawler, execute the generated `crawler` executable followed by the URL of the website you want to crawl:
./crawler <website> <max_depth>

For example:
./crawler https://example.com 2

You can interrupt the program at any time by sending a SIGINT signal (typically Ctrl+C). This will gracefully terminate the program, cleaning up resources and saving any fetched data.

## Makefile Targets

- `all`: Compiles the code to generate the `crawler` executable.
- `clean`: Removes the `crawler` executable and any intermediate files.
- `run`: Compiles the code and runs the crawler with a specified URL.

## Notes

- Ensure that libcurl is installed on your system and accessible to the compiler. You may need to adjust the compiler and linker flags in the Makefile if libcurl is installed in a non-standard location.