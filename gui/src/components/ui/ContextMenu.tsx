import { useEffect, useRef, useState, type MouseEvent as ReactMouseEvent } from "react";
import { createPortal } from "react-dom";
import { FontAwesomeIcon } from "@/icons";
import { faChevronRight } from "@fortawesome/free-solid-svg-icons";
import type { IconDefinition } from "@fortawesome/fontawesome-svg-core";

export interface ContextMenuItem {
  id: string;
  label: string;
  onClick?: () => void;
  icon?: IconDefinition;
  disabled?: boolean;
  danger?: boolean;
  checked?: boolean;
  children?: ContextMenuItem[];
}

export interface ContextMenuProps {
  x: number;
  y: number;
  items: ContextMenuItem[];
  onClose: () => void;
}

interface ContextMenuListProps {
  items: ContextMenuItem[];
  onClose: () => void;
  nested?: boolean;
}

function ContextMenuList({ items, onClose, nested = false }: ContextMenuListProps) {
  const [openSubmenuId, setOpenSubmenuId] = useState<string | null>(null);
  const closeSubmenuTimerRef = useRef<number | null>(null);

  const clearCloseSubmenuTimer = () => {
    if (closeSubmenuTimerRef.current !== null) {
      window.clearTimeout(closeSubmenuTimerRef.current);
      closeSubmenuTimerRef.current = null;
    }
  };

  const scheduleCloseSubmenu = () => {
    clearCloseSubmenuTimer();
    closeSubmenuTimerRef.current = window.setTimeout(() => {
      setOpenSubmenuId(null);
      closeSubmenuTimerRef.current = null;
    }, 200);
  };

  useEffect(() => () => clearCloseSubmenuTimer(), []);

  return (
    <ul
      className={nested ? "avar-context-menu__submenu" : "avar-context-menu__list"}
      role="menu"
    >
      {items.map((item) => {
        if (item.children && item.children.length > 0) {
          return (
            <li
              key={item.id}
              role="none"
              className="avar-context-menu__submenu-wrap"
              onMouseEnter={() => {
                if (!item.disabled) {
                  clearCloseSubmenuTimer();
                  setOpenSubmenuId(item.id);
                }
              }}
              onMouseLeave={(event: ReactMouseEvent<HTMLLIElement>) => {
                const related = event.relatedTarget;
                if (related instanceof Node && event.currentTarget.contains(related)) {
                  return;
                }
                scheduleCloseSubmenu();
              }}
            >
              <button
                type="button"
                role="menuitem"
                aria-haspopup="menu"
                aria-expanded={openSubmenuId === item.id}
                className="avar-context-menu__item"
                disabled={item.disabled}
              >
                {item.icon ? (
                  <FontAwesomeIcon icon={item.icon} className="avar-context-menu__icon" />
                ) : (
                  <span
                    className="avar-context-menu__icon avar-context-menu__icon--spacer"
                    aria-hidden="true"
                  />
                )}
                <span className="avar-context-menu__label">{item.label}</span>
                <FontAwesomeIcon icon={faChevronRight} className="avar-context-menu__chevron" />
              </button>
              {openSubmenuId === item.id ? (
                <ContextMenuList
                  items={item.children}
                  onClose={onClose}
                  nested
                />
              ) : null}
            </li>
          );
        }

        return (
          <li key={item.id} role="none">
            <button
              type="button"
              role="menuitem"
              className={`avar-context-menu__item ${item.danger ? "avar-context-menu__item--danger" : ""}`}
              disabled={item.disabled}
              onClick={() => {
                if (!item.disabled && item.onClick) {
                  item.onClick();
                  onClose();
                }
              }}
            >
              {item.icon ? (
                <FontAwesomeIcon icon={item.icon} className="avar-context-menu__icon" />
              ) : item.checked ? (
                <span className="avar-context-menu__icon" aria-hidden="true">
                  ✓
                </span>
              ) : (
                <span
                  className="avar-context-menu__icon avar-context-menu__icon--spacer"
                  aria-hidden="true"
                />
              )}
              <span className="avar-context-menu__label">{item.label}</span>
            </button>
          </li>
        );
      })}
    </ul>
  );
}

export function ContextMenu({ x, y, items, onClose }: ContextMenuProps) {
  const menuRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    function handlePointerDown(event: MouseEvent) {
      if (menuRef.current?.contains(event.target as Node)) {
        return;
      }
      onClose();
    }

    function handleKeyDown(event: KeyboardEvent) {
      if (event.key === "Escape") {
        onClose();
      }
    }

    window.addEventListener("mousedown", handlePointerDown);
    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("mousedown", handlePointerDown);
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [onClose]);

  useEffect(() => {
    const menu = menuRef.current;
    if (!menu) {
      return;
    }

    const rect = menu.getBoundingClientRect();
    const maxX = window.innerWidth - rect.width - 8;
    const maxY = window.innerHeight - rect.height - 8;
    menu.style.left = `${Math.min(x, maxX)}px`;
    menu.style.top = `${Math.min(y, maxY)}px`;
  }, [x, y]);

  return createPortal(
    <div
      ref={menuRef}
      className="avar-context-menu"
      style={{ left: x, top: y }}
      onContextMenu={(e) => e.preventDefault()}
    >
      <ContextMenuList items={items} onClose={onClose} />
    </div>,
    document.body,
  );
}
