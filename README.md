# Infos utiles

## Commande de compilation :

```bash
gcc -Wall -o othello_GUI othello_GUI.c $(pkg-config --cflags --libs gtk+-3.0)
```

## Lancement projet avec <num_port> le port TCP d'écoute :

`./othello_GUI <num_port>`

