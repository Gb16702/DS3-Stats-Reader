import { TanStackDevtools } from "@tanstack/react-devtools";
import type { QueryClient } from "@tanstack/react-query";
import { createRootRouteWithContext, Outlet } from "@tanstack/react-router";
import { QueryDevtools } from "@/providers/query";
import { RouterDevtools } from "@/providers/router";

interface MyRouterContext {
    queryClient: QueryClient;
}

export const Route = createRootRouteWithContext<MyRouterContext>()({
    component: RootComponent,
});

function RootComponent() {
    return (
        <>
            <Outlet />
            {import.meta.env.DEV && (
                <TanStackDevtools
                    config={{ position: "bottom-right" }}
                    plugins={[QueryDevtools, RouterDevtools]}
                />
            )}
        </>
    );
}
