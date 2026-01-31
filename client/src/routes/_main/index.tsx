import { createFileRoute } from "@tanstack/react-router";

export const Route = createFileRoute("/_main/")({
    component: App,
});

function App() {
    return (
        <div>
            <h1>Ember</h1>
        </div>
    );
}
