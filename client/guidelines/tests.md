# Tests

> Les tests protègent les règles métier et vérifient que l'orchestration technique appelle correctement ces règles.

## Principe fondamental

Distingue clairement :

- **Tests unitaires** : protègent les règles métier
- **Tests E2E** : vérifient l'orchestration et l'intégration technique

## Tests unitaires

### Objectif

Protéger les **règles métier**. Un test unitaire doit échouer si une règle produit change.

### Contraintes

1. **Pas de couplage au framework ou à l'I/O**
2. **Éviter les tests qui vérifient uniquement des valeurs définies dans le test**
3. **Tester le comportement, pas l'implémentation**

### ✅ Bon test unitaire

```typescript
// pricing.test.ts
describe('validatePriceRange', () => {
  it('rejects when min price is negative', () => {
    const result = validatePriceRange(-10, 100)
    expect(result.success).toBe(false)
    expect(result.error).toBe('NEGATIVE_PRICE')
  })

  it('rejects when max is less than min', () => {
    const result = validatePriceRange(100, 50)
    expect(result.success).toBe(false)
    expect(result.error).toBe('INVALID_RANGE')
  })

  it('accepts valid price range', () => {
    const result = validatePriceRange(50, 100)
    expect(result.success).toBe(true)
    expect(result.data).toEqual({ min: 50, max: 100 })
  })
})
```

### ❌ Mauvais test unitaire

```typescript
// ❌ Teste une valeur définie dans le test, pas une règle métier
it('returns the input', () => {
  const result = formatPrice(1000)
  expect(result).toBe('€10.00') // Juste un echo, pas une règle
})

// ❌ Couplé au framework
it('renders the component', () => {
  render(<PricingForm />) // Test unitaire couplé à React
  expect(screen.getByText('Save')).toBeInTheDocument()
})
```

## Tests E2E

### Objectif

Vérifier que l'**orchestration technique** appelle correctement les règles métier.

### Ce qu'ils testent

- L'intégration entre composants
- Les appels API
- Le flux utilisateur complet

```typescript
// pricing.e2e.test.ts
describe('Service Pricing Configuration', () => {
  it('allows admin to set price ranges for services', async () => {
    await page.goto('/profile/settings?tab=organization')
    await page.click('text=Manage Pricing')

    await page.fill('[data-testid="min-price-service-1"]', '100')
    await page.fill('[data-testid="max-price-service-1"]', '500')

    await page.click('text=Save')

    await expect(page.locator('text=Pricing saved')).toBeVisible()
  })
})
```

## Règle de décision

| Question | Type de test |
|----------|--------------|
| Est-ce une règle métier pure ? | Unitaire |
| Est-ce de l'I/O ou du framework ? | E2E |
| Peut-il tourner sans serveur/browser ? | Unitaire |
| Nécessite-t-il une vraie DB/API ? | E2E |

## Si un test n'apporte pas de protection réelle

**Supprime-le.**

Un test inutile :

- Ajoute du bruit
- Ralentit la CI
- Donne une fausse confiance
- Coûte en maintenance
