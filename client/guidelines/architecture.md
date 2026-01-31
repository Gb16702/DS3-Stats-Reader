# Architecture

> Organisation du code pour isoler les décisions importantes des détails techniques et maîtriser la complexité.

## Principe fondamental

Implémente le code avec une architecture claire qui **sépare les décisions importantes des détails techniques**.

Les règles et comportements essentiels du produit doivent être :

- Isolés
- Indépendants des frameworks
- Indépendants de l'infrastructure
- Indépendants des mécanismes de transport

Les parties techniques doivent **uniquement exécuter et orchestrer** ces décisions, sans en contenir.

## Règle de dépendance

```
┌─────────────────────────────────────┐
│           Routes / UI               │  ← Orchestration
├─────────────────────────────────────┤
│           Features                  │  ← Logique métier
├─────────────────────────────────────┤
│     Components / Infrastructure     │  ← Détails techniques
└─────────────────────────────────────┘
```

Les flèches de dépendance vont toujours vers le bas.
Le métier ne dépend JAMAIS de la technique.

## En cas d'ambiguïté

Si une responsabilité est ambiguë, pose-toi ces questions :

1. **Est-ce une règle produit ?** → `features/`
2. **Est-ce de l'orchestration ?** → `routes/`
3. **Est-ce de l'UI pure ?** → `components/ui/`
4. **Est-ce un détail technique remplaçable ?** → `infrastructure/`

Explique toujours où le code doit vivre et pourquoi.
