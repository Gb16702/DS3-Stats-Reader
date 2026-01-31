# Types vs Runtime

> Les types décrivent des intentions à la compilation, mais seules les validations runtime protègent réellement le système.

## Principe fondamental

Distingue clairement :

- **Ce qui est garanti par le système de types** (compile-time)
- **Ce qui doit être validé au runtime**

**Ne suppose jamais qu'un type TypeScript protège contre des données invalides venant de l'extérieur.**

## Les types ne protègent PAS contre

- Les données d'une API externe
- Les inputs utilisateur
- Les paramètres d'URL
- Les données de localStorage
- Tout ce qui vient de l'extérieur du système TypeScript

## Règle d'or

Frontière du système = Validation runtime obligatoire

### Frontières typiques

| Source | Validation nécessaire ? |
|--------|------------------------|
| API externe | ✅ Oui |
| Input utilisateur | ✅ Oui |
| URL params / query | ✅ Oui |
| localStorage / cookies | ✅ Oui |
| Fonction interne typée | ❌ Non (le type suffit) |
| Import depuis un module typé | ❌ Non |

## Exemples

### ✅ Validation aux frontières

```typescript
// services/api.ts - Frontière API
import { z } from 'zod'

const ServicePricingSchema = z.object({
  serviceId: z.string(),
  minPrice: z.number().nullable(),
  maxPrice: z.number().nullable(),
  currency: z.string(),
})

type ServicePricing = z.infer<typeof ServicePricingSchema>

export async function fetchServicePricing(orgId: string): Promise<ServicePricing[]> {
  const response = await fetch(`/api/organizations/${orgId}/pricing`)
  const data = await response.json()

  // ✅ Validation runtime - on ne fait pas confiance à l'API
  return z.array(ServicePricingSchema).parse(data)
}
```

### ✅ Pas de validation interne

```typescript
// pricing.ts - Logique métier interne
// Le type suffit, pas besoin de validation runtime

export function calculateTotal(services: ServicePricing[]): number {
  // Pas de validation ici - le type garantit la structure
  return services.reduce((sum, s) => {
    const price = s.minPrice ?? 0
    return sum + price
  }, 0)
}
```

### ❌ Fausse sécurité

```typescript
// ❌ Le type ne protège PAS des données invalides
interface ApiResponse {
  services: ServicePricing[]
}

async function fetchPricing(): Promise<ApiResponse> {
  const response = await fetch('/api/pricing')
  return response.json() as ApiResponse // ❌ Cast dangereux !
  // Si l'API retourne autre chose, TypeScript ne le saura pas
}
```

## Pattern recommandé avec Zod

```typescript
// 1. Définir le schema (source de vérité)
const PriceRangeSchema = z.object({
  min: z.number().min(0),
  max: z.number().min(0),
}).refine(data => data.max >= data.min, {
  message: 'Max must be greater than or equal to min'
})

// 2. Dériver le type du schema
type PriceRange = z.infer<typeof PriceRangeSchema>

// 3. Valider aux frontières
function validateUserInput(input: unknown): PriceRange {
  return PriceRangeSchema.parse(input)
}

// 4. Utiliser le type en interne (pas de revalidation)
function formatPriceRange(range: PriceRange): string {
  return `${range.min} - ${range.max}`
}
```

## Résumé

| Situation | Action |
|-----------|--------|
| Données externes | Valider avec Zod/schema |
| Données internes typées | Faire confiance au type |
| Doute sur l'origine | Valider |
| Performance critique | Valider une fois à l'entrée |
