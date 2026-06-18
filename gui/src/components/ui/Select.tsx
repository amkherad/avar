import {
  Children,
  isValidElement,
  useEffect,
  useId,
  useLayoutEffect,
  useRef,
  useState,
  type ChangeEvent,
  type CSSProperties,
  type MouseEvent as ReactMouseEvent,
  type ReactNode,
  type SelectHTMLAttributes,
} from "react";
import { createPortal } from "react-dom";
import { FontAwesomeIcon } from "@/icons";
import { faCheck, faChevronDown } from "@fortawesome/free-solid-svg-icons";

export interface SelectOption {
  value: string;
  label: string;
  disabled?: boolean;
}

export interface SelectProps extends Omit<SelectHTMLAttributes<HTMLSelectElement>, "children"> {
  label?: string;
  /** Visually compact trigger for toolbars and table headers. */
  compact?: boolean;
  children: ReactNode;
}

function parseOptions(children: ReactNode): SelectOption[] {
  const options: SelectOption[] = [];
  Children.forEach(children, (child) => {
    if (!isValidElement<{ value?: string; children?: ReactNode; disabled?: boolean }>(child)) {
      return;
    }
    if (child.type === "option") {
      const value = String(child.props.value ?? "");
      const label =
        typeof child.props.children === "string"
          ? child.props.children
          : Children.toArray(child.props.children).join("");
      options.push({
        value,
        label,
        disabled: child.props.disabled,
      });
    }
  });
  return options;
}

export function Select({
  label,
  id,
  className = "",
  compact = false,
  children,
  value,
  defaultValue,
  disabled,
  onChange,
  name,
  required,
  onClick,
}: SelectProps) {
  const autoId = useId();
  const selectId = id ?? (label ? `select-${label.replace(/\s+/g, "-").toLowerCase()}` : autoId);
  const options = parseOptions(children);
  const [open, setOpen] = useState(false);
  const [fixedMenuStyle, setFixedMenuStyle] = useState<CSSProperties | undefined>();
  const containerRef = useRef<HTMLDivElement>(null);
  const menuRef = useRef<HTMLUListElement>(null);

  const currentValue = value !== undefined ? String(value) : defaultValue !== undefined ? String(defaultValue) : options[0]?.value ?? "";
  const selected = options.find((option) => option.value === currentValue) ?? options[0];

  function updateFixedMenuPosition(): CSSProperties | undefined {
    if (!compact || !containerRef.current) {
      return undefined;
    }
    const rect = containerRef.current.getBoundingClientRect();
    return {
      position: "fixed",
      top: rect.bottom + 4,
      left: rect.left,
      width: Math.max(rect.width, 160),
      zIndex: 200,
    };
  }

  function toggleDropdown(event?: React.MouseEvent<HTMLButtonElement>) {
    event?.stopPropagation();
    if (disabled) {
      return;
    }
    setOpen((prev) => {
      const next = !prev;
      if (next) {
        setFixedMenuStyle(updateFixedMenuPosition());
      } else {
        setFixedMenuStyle(undefined);
      }
      return next;
    });
  }

  useLayoutEffect(() => {
    if (!open || !compact) {
      return;
    }

    function refreshMenuPosition() {
      setFixedMenuStyle(updateFixedMenuPosition());
    }

    refreshMenuPosition();
    window.addEventListener("resize", refreshMenuPosition);
    window.addEventListener("scroll", refreshMenuPosition, true);
    return () => {
      window.removeEventListener("resize", refreshMenuPosition);
      window.removeEventListener("scroll", refreshMenuPosition, true);
    };
  }, [open, compact]);

  useEffect(() => {
    if (!open) {
      return;
    }

    function handleClickOutside(event: MouseEvent) {
      const target = event.target as Node;
      if (
        containerRef.current?.contains(target) ||
        menuRef.current?.contains(target)
      ) {
        return;
      }
      setOpen(false);
      setFixedMenuStyle(undefined);
    }

    function handleEscape(event: KeyboardEvent) {
      if (event.key === "Escape") {
        setOpen(false);
        setFixedMenuStyle(undefined);
      }
    }

    document.addEventListener("mousedown", handleClickOutside);
    document.addEventListener("keydown", handleEscape);
    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
      document.removeEventListener("keydown", handleEscape);
    };
  }, [open]);

  function emitChange(nextValue: string) {
    onChange?.({
      target: { value: nextValue, name: name ?? "" },
      currentTarget: { value: nextValue, name: name ?? "" },
    } as ChangeEvent<HTMLSelectElement>);
  }

  function selectOption(option: SelectOption) {
    if (option.disabled) {
      return;
    }
    emitChange(option.value);
    setFixedMenuStyle(undefined);
    setOpen(false);
  }

  const menu = open ? (
    <ul
      ref={menuRef}
      className={`avar-dropdown__menu ${compact ? "avar-dropdown__menu--fixed" : ""}`.trim()}
      style={compact ? fixedMenuStyle : undefined}
      role="listbox"
      aria-labelledby={selectId}
    >
      {options.map((option) => {
        const isSelected = option.value === currentValue;
        return (
          <li key={option.value} role="presentation">
            <button
              type="button"
              role="option"
              aria-selected={isSelected}
              className={`avar-dropdown__option ${isSelected ? "avar-dropdown__option--selected" : ""}`}
              disabled={option.disabled}
              onClick={() => selectOption(option)}
            >
              <span className="avar-dropdown__option-label">{option.label}</span>
              {isSelected ? (
                <FontAwesomeIcon icon={faCheck} className="avar-dropdown__option-check" />
              ) : null}
            </button>
          </li>
        );
      })}
    </ul>
  ) : null;

  return (
    <label className={`avar-field ${compact ? "avar-field--compact" : ""} ${className}`.trim()} htmlFor={selectId}>
      {label ? <span className="avar-field__label">{label}</span> : null}
      <div className={`avar-dropdown ${compact ? "avar-dropdown--compact" : ""}`} ref={containerRef}>
        <button
          id={selectId}
          type="button"
          className="avar-dropdown__trigger"
          aria-haspopup="listbox"
          aria-expanded={open}
          disabled={disabled}
          onClick={(event) => {
            if (onClick) {
              onClick(event as unknown as ReactMouseEvent<HTMLSelectElement>);
            }
            if (!event.defaultPrevented) {
              toggleDropdown(event);
            }
          }}
        >
          <span className="avar-dropdown__value">{selected?.label ?? ""}</span>
          <span className={`avar-dropdown__chevron ${open ? "avar-dropdown__chevron--open" : ""}`}>
            <FontAwesomeIcon icon={faChevronDown} />
          </span>
        </button>

        {compact && menu ? createPortal(menu, document.body) : menu}
      </div>

      {name ? (
        <input type="hidden" name={name} value={currentValue} required={required} readOnly />
      ) : null}
    </label>
  );
}
