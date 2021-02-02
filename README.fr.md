[English](README.md) | [Français](README.fr.md)

# Projet SE et Virtualisation 

## Contexte du projet

à définir

## Compilation et exécution

Afin de compiler le projet, il suffit d'exécuter dans le repertoire la
commande :

```bash
gcc -Wall -o othello_GUI othello_GUI.c $(pkg-config --cflags --libs gtk+-3.0)
```

### Lancement projet avec <num_port> le port TCP d'écoute :

```bash
./othello_GUI <num_port>
```
