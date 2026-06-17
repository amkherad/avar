export function Spinner({ label = "Loading" }: { label?: string }) {
  return (
    <div className="avar-spinner" role="status" aria-label={label}>
      <span className="avar-spinner__ring" />
    </div>
  );
}
