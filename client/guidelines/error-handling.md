# Gestion d'Erreurs

> Une erreur métier est un résultat attendu du domaine ; une erreur technique est un incident. On ne les représente, ne les propage et ne les traite jamais de la même manière.

## Principe fondamental

Sépare **strictement** erreurs métier et erreurs techniques.

| Type | Représentation | Exemple |
|------|----------------|---------|
| **Erreur métier** | `return` (résultat explicite) | Prix invalide, email déjà utilisé |
| **Erreur technique** | `throw` (exception) | Réseau down, DB inaccessible |

## Règles

### 1. Erreurs métier = Résultats explicites

```typescript
// ✅ Correct : erreur métier retournée
type ValidationResult =
  | { success: true; data: PriceRange }
  | { success: false; error: 'INVALID_RANGE' | 'NEGATIVE_PRICE' }

function validatePriceRange(min: number, max: number): ValidationResult {
  if (min < 0) return { success: false, error: 'NEGATIVE_PRICE' }
  if (max < min) return { success: false, error: 'INVALID_RANGE' }
  return { success: true, data: { min, max } }
}
```

```typescript
// ❌ Incorrect : erreur métier en exception
function validatePriceRange(min: number, max: number): PriceRange {
  if (min < 0) throw new Error('NEGATIVE_PRICE') // ❌
  // ...
}
```

### 2. Erreurs techniques = Exceptions

```typescript
// ✅ Correct : erreur technique levée
async function fetchPricing(orgId: string): Promise<Pricing[]> {
  const response = await fetch(`/api/pricing/${orgId}`)
  if (!response.ok) {
    throw new Error(`Failed to fetch pricing: ${response.status}`)
  }
  return response.json()
}
```

### 3. try/catch aux frontières uniquement

Les try/catch sont placés uniquement là où une décision peut être prise :

- UI (composants React)
- Controllers
- API routes

```typescript
// ✅ Correct : try/catch à la frontière UI
const PricingForm = () => {
  const handleSubmit = async () => {
    try {
      await savePricing(data)
      showSuccessToast('Saved')
    } catch (error) {
      showErrorToast('Failed to save')
    }
  }
}
```

```typescript
// ❌ Incorrect : try/catch dans le métier
function validatePriceRange(min: number, max: number) {
  try { // ❌ Pas de décision possible ici
    // ...
  } catch (e) {
    // Que faire ? Le métier ne sait pas.
  }
}
```

### 4. Nommer clairement

Les erreurs doivent porter une intention claire :

```typescript
// ✅ Clair
type PriceError = 'PRICE_BELOW_MINIMUM' | 'MAX_LESS_THAN_MIN' | 'CURRENCY_MISMATCH'

// ❌ Vague
type PriceError = 'ERROR' | 'INVALID' | 'FAILED'
```

## Résumé

- Ne mélange jamais logique métier et gestion d'incidents
- Le métier retourne des résultats (succès ou échec métier)
- La technique peut lever des exceptions
- Les frontières attrapent et décident
