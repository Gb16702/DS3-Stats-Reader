import { createFileRoute, Outlet } from "@tanstack/react-router";

function Header() {
    return (
        <header className="fixed inset-x-0 top-0 h-10 lg:h-12 bg-ds-background-100" />
    );
}

function Footer() {
    return <footer className="bg-ds-background-100" />;
}

export const Route = createFileRoute("/_main")({
    component: MainLayout,
});

function MainLayout() {
    return (
        <div className="flex min-h-dvh flex-col bg-ds-background-100">
            <Header />
            <main className="flex-1 pt-10 lg:pt-12 px-2">
                <div className="flex-1 min-h-dvh border border-ds-border-default bg-ds-background-200 rounded-sm">
                    <Outlet />
                </div>
            </main>
            <Footer />
        </div>
    );
}
