# Nommage & Lisibilité

> Un bon nom exprime l'intention métier, réduit la charge mentale et protège le code contre les refactors et les erreurs futures.

## Principe fondamental

Les noms doivent refléter **l'intention métier**, pas l'implémentation technique.

## Règles

### 1. Nommer par intention, pas par type

```typescript
// ✅ Intention claire
const selectedServices: string[]
const priceRange: { min: number; max: number }
const canEditPricing: boolean

// ❌ Nommé par type/implémentation
const serviceArray: string[]
const priceObject: { min: number; max: number }
const pricingBoolean: boolean
```

### 2. Les fonctions décrivent une action

```typescript
// ✅ Action claire
function validatePriceRange(min: number, max: number)
function formatPriceForDisplay(cents: number, currency: string)
function calculateTotalWithVAT(services: Service[], vatRate: number)

// ❌ Vague ou technique
function process(data: any)
function handleData(input: unknown)
function doValidation(x: number, y: number)
```

### 3. Les booléens sont des questions

```typescript
// ✅ Question claire
const isValidRange = max >= min
const hasUnsavedChanges = !isEqual(current, saved)
const canUserEditPricing = isAdminOrOwner(role)

// ❌ Pas une question
const valid = max >= min
const changes = !isEqual(current, saved)
const edit = isAdminOrOwner(role)
```

### 4. Les erreurs portent leur cause

```typescript
// ✅ Cause explicite
type PricingError =
  | 'PRICE_BELOW_MINIMUM'
  | 'MAX_LESS_THAN_MIN'
  | 'CURRENCY_NOT_SUPPORTED'
  | 'SERVICE_NOT_FOUND'

// ❌ Vague
type PricingError = 'ERROR' | 'INVALID' | 'FAILED' | 'BAD_INPUT'
```

### 5. Éviter les abréviations

```typescript
// ✅ Lisible
const organizationId: string
const selectedCategories: string[]
const minimumPrice: number

// ❌ Abrégé
const orgId: string      // Acceptable si convention établie
const selCats: string[]  // ❌ Jamais
const minPx: number      // ❌ Jamais
```

**Exception** : Les abréviations universellement comprises sont OK (`id`, `url`, `api`, `db`)

### 6. Cohérence dans le codebase

Utilise les mêmes termes partout :

```typescript
// Si tu utilises "pricing" quelque part...
// ✅ Cohérent
features/pricing/
useServicePricing()
ServicePricingDialog
pricingApi

// ❌ Incohérent
features/prices/        // pricing vs prices
useServiceCosts()       // pricing vs costs
ServiceRatesDialog      // pricing vs rates
```

## Checklist

Avant de nommer, demande-toi :

- [ ] Est-ce que quelqu'un comprend l'intention sans lire l'implémentation ?
- [ ] Est-ce que le nom survivrait à un refactor ?
- [ ] Est-ce cohérent avec le reste du codebase ?
- [ ] Est-ce que je peux le lire à voix haute sans confusion ?
