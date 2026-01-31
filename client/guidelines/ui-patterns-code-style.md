# UI Patterns & Code Style

## Code Style

### No Comments

Never add comments to the code. Code should be self-documenting through clear naming and structure.

### Use Existing Components

Always favor existing components over native HTML elements for consistency:

- `<Button>` instead of `<button>`
- `<Input>` instead of `<input>`
- etc.

## Pending UI States

### Form Submission (mutations)

For submit buttons during form submission:

- Show `<Spinner />` component only
- No text alongside the spinner
- Example:

```tsx
<Button disabled={isPending}>
  {isPending ? <Spinner /> : "Submit"}
</Button>
```

### Data Fetching (GET requests)

For data being loaded from the server:

- Use `<Skeleton />` component to wrap dynamic content
- Create page-specific skeleton components when needed
- Never use text like "Loading...", "Loading", or any loading text

Example:

```tsx
function UserCardSkeleton() {
  return (
    <Card>
      <Skeleton className="h-6 w-32" />
      <Skeleton className="h-4 w-48" />
    </Card>
  )
}
```

## Rendering Strategy

### SSR (Server-Side Rendering)

Use SSR when:

- SEO is important (public pages, marketing, blog)
- Content needs to be indexed by search engines

Implementation with React Router loader:

```tsx
export async function loader({ params }: Route.LoaderArgs) {
  const data = await fetchData(params.id)
  return { data }
}
```

### CSR (Client-Side Rendering)

Use CSR when:

- SEO is not needed (authenticated dashboard pages)
- Page is behind authentication
- Interactive features are the primary focus

**Preferred approach: TanStack Query with `useSuspenseQuery`**

```tsx
import { useSuspenseQuery } from "@tanstack/react-query"

const STALE_TIME = 5 * 60 * 1000

function ContentWrapper({ id }: { id: string }) {
  const { data } = useSuspenseQuery({
    queryKey: ["resource", id],
    queryFn: () => fetchResource(id),
    staleTime: STALE_TIME,
  })

  return <Content data={data} />
}

export default function Page({ params }: Route.ComponentProps) {
  return (
    <Suspense fallback={<PageSkeleton />}>
      <ContentWrapper id={params.id} />
    </Suspense>
  )
}
```

### staleTime Configuration

`staleTime` = duration data is considered "fresh" (no refetch)

Common values:

- `5 * 60 * 1000` (5 min) - default for most data
- `10 * 60 * 1000` (10 min) - data that changes rarely
- `Infinity` - never stale (manual refetch only)

After `staleTime`, TanStack Query refetches in background on next access.

### Benefits of TanStack Query over React Router loaders (for CSR)

- Automatic caching (revisiting = instant load)
- Background revalidation
- No loader needed
- Better devtools

Only add caching when it provides real value - don't cache everything by default.
