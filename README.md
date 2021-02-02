# Operating Systems and Virtualization Project

## Project context

To be defined.

## Build and execution

To build the project, run this command in the project directory:

```bash
gcc -Wall -o othello_GUI othello_GUI.c $(pkg-config --cflags --libs gtk+-3.0)
```

### Start the project with <num_port> as the TCP listening port

```bash
./othello_GUI <num_port>
```
