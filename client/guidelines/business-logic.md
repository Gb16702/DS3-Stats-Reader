# Logique Métier

> Règles du produit indépendantes de la technologie.

## Principe fondamental

Sépare explicitement :

- **Logique métier** : règles produit, indépendantes de la technologie
- **Logique technique** : framework, I/O, stockage

## Règle d'or

La logique technique appelle le métier.
Jamais l'inverse.

## Exemples

### ✅ Correct

```typescript
// features/pricing/pricing.ts (métier)
export function validatePriceRange(min: number, max: number): boolean {
  return min >= 0 && max >= min
}

// routes/settings.tsx (technique/orchestration)
import { validatePriceRange } from '@/features/pricing/pricing'

const handleSave = () => {
  if (!validatePriceRange(minPrice, maxPrice)) {
    showError('Invalid price range')
    return
  }
  // ...
}
```

### ❌ Incorrect

```typescript
// features/pricing/pricing.ts
import { showToast } from '@/components/ui/toasts' // ❌ Dépendance technique

export function validateAndNotify(min: number, max: number) {
  if (min < 0) {
    showToast('Min must be positive') // ❌ Le métier appelle la technique
    return false
  }
  // ...
}
```

## Structure dans une feature

```
features/pricing/
  pricing.ts          ← Types, règles, helpers métier (CŒUR)
  components/         ← UI spécifique à la feature
  hooks/              ← Hooks métier
  services/           ← Appels API (technique)
```

Le fichier `pricing.ts` est le cœur métier. Il ne doit importer :

- Aucun composant React
- Aucun hook
- Aucune dépendance I/O
- Aucun framework
