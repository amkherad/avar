import { Component, type ErrorInfo, type ReactNode } from "react";
import { Button } from "@/components/ui/Button";
import { appLogger } from "@/lib/appLogger";

export interface ErrorBoundaryProps {
  children: ReactNode;
  /** Short label for logs and the fallback heading. */
  name?: string;
  resetLabel?: string;
  /** Optional custom fallback; receives reset callback. */
  fallback?: (reset: () => void) => ReactNode;
}

interface ErrorBoundaryState {
  error: Error | null;
}

export class ErrorBoundary extends Component<ErrorBoundaryProps, ErrorBoundaryState> {
  state: ErrorBoundaryState = { error: null };

  static getDerivedStateFromError(error: Error): ErrorBoundaryState {
    return { error };
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    const label = this.props.name ?? "component";
    appLogger.gui.error(`${label} crashed`, error.message);
    if (info.componentStack) {
      appLogger.gui.debug(`${label} stack`, info.componentStack);
    }
  }

  reset = () => {
    this.setState({ error: null });
  };

  render() {
    const { error } = this.state;
    if (!error) {
      return this.props.children;
    }

    if (this.props.fallback) {
      return this.props.fallback(this.reset);
    }

    const label = this.props.name ?? "This section";
    const resetLabel = this.props.resetLabel ?? "Try again";

    return (
      <div className="avar-error-boundary" role="alert">
        <p className="avar-error-boundary__message">
          {label} encountered an error: {error.message}
        </p>
        <Button size="sm" variant="secondary" onClick={this.reset}>
          {resetLabel}
        </Button>
      </div>
    );
  }
}
