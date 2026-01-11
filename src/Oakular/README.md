# Behavior Tree Editor

Version 2.0 - Architecture modulaire

## Overview

Éditeur graphique pour les arbres de comportement avec deux modes :
- **Editor** : Créer et éditer des arbres visuellement
- **Visualizer** : Visualiser en temps réel l'exécution d'arbres distants via TCP

## Architecture

### Structure des fichiers

```
src/Editor/
├── main.cpp                    # Point d'entrée
├── BehaviorTreeEditor.hpp/cpp  # Classe principale (hérite OpenGLApplication)
├── EditorNode.hpp              # Structures de données (EditorNode, EditorLink, etc.)
├── NodeRenderer.hpp/cpp        # Rendu des nœuds avec ImNodes
├── VisualizerServer.hpp/cpp    # Serveur TCP avec SFML
└── CMakeLists.txt              # Configuration de compilation
```

### Composants

#### 1. BehaviorTreeEditor (Classe principale)

Hérite de `robotik::renderer::OpenGLApplication` pour gérer :
- La fenêtre OpenGL
- L'interface ImGui
- La boucle de rendu

**Responsabilités** :
- Gestion des modes (Editor/Visualizer)
- Coordination entre les composants
- Gestion des menus et des interfaces utilisateur
- Opérations sur les nœuds (ajout, suppression, liens)

#### 2. EditorNode (Structures de données)

Définit les types et structures :
- `BTNodeType` : Énumération des types de nœuds
- `EditorNode` : Représentation d'un nœud dans l'éditeur
- `EditorLink` : Lien entre deux nœuds
- `EditorMode` : Mode de l'éditeur

#### 3. NodeRenderer (Rendu)

Utilise **ImNodes** pour le rendu graphique :
- Initialisation du contexte ImNodes
- Rendu des nœuds avec codes couleurs par type
- Gestion des interactions (sélection, création/suppression de liens)
- Gestion des pins d'entrée/sortie

#### 4. VisualizerServer (Serveur TCP)

Utilise **SFML Network** pour :
- Écouter les connexions TCP entrantes (port 8888 par défaut)
- Recevoir des données YAML depuis des clients
- Mode non-bloquant pour ne pas geler l'interface

## Dépendances

- **OpenGL** : Rendu graphique
- **GLFW** : Gestion de fenêtre
- **GLEW** : Extensions OpenGL
- **ImGui** : Interface utilisateur
- **ImNodes** : Éditeur de graphes de nœuds
- **SFML Network** : Communication TCP

## Compilation

```bash
mkdir build
cd build
cmake ..
make
```

## Utilisation

### Mode Editor

1. Lancer l'application
2. Menu → Mode → Editor
3. Clic droit ou Espace : Ouvrir la palette de nœuds
4. Créer des liens en glissant depuis les pins de sortie vers les pins d'entrée
5. Supprimer : Sélectionner un nœud et appuyer sur Delete
6. Auto Layout : Menu → Edit → Auto Layout

### Mode Visualizer

1. Menu → Mode → Visualizer
2. Le serveur démarre automatiquement sur le port 8888
3. Connecter un client pour envoyer des données YAML
4. L'arbre s'affiche automatiquement à réception

### Raccourcis clavier

- `Ctrl+O` : Charger un fichier YAML
- `Ctrl+S` : Sauvegarder en YAML
- `Ctrl+L` : Auto Layout
- `Space` : Ouvrir la palette de nœuds
- `Delete` : Supprimer le nœud sélectionné
- `Ctrl+Q` : Quitter

## Protocole TCP (Mode Visualizer)

### Format des messages

Le serveur attend des données YAML au format suivant :

```yaml
type: Sequence
name: RootSequence
children:
  - type: Action
    name: MoveForward
  - type: Condition
    name: IsObstacleDetected
---
```

Le marqueur `---` ou `EOF` indique la fin du message.

### Exemple de client Python

```python
import socket

def send_tree(yaml_data):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 8888))
    sock.sendall(yaml_data.encode() + b'\n---\n')
    sock.close()
```

## Amélioration futures

- [ ] Implémenter YamlLoader pour charger/sauvegarder des arbres
- [ ] Parser automatiquement le YAML reçu en mode Visualizer
- [ ] Ajouter la validation des liens (pas de cycles)
- [ ] Exporter vers des images (PNG, SVG)
- [ ] Undo/Redo
- [ ] Copier/Coller de sous-arbres
- [ ] Bibliothèque de templates
- [ ] Débogage pas à pas en mode Visualizer

## Différences avec v1

### Version 1 (BehaviorTreeEditor.cpp_old)
- Tout dans un seul fichier (~1873 lignes)
- Pas d'héritage de classe de base
- Gestion manuelle du rendu des ports et des liens
- Code plus complexe et difficile à maintenir

### Version 2 (Actuelle)
- Architecture modulaire (~150 lignes par fichier)
- Héritage de OpenGLApplication
- Délégation du rendu à NodeRenderer
- Séparation claire des responsabilités
- Plus facile à tester et à étendre

## License

MIT License - Copyright (c) 2025 Quentin Quadrat

